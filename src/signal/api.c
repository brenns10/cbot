/*
 * Signal(d) API functions
 */
#include <stdlib.h>

#include "internal.h"

void sig_get_profile(struct cbot_signal_backend *sig, char *phone)
{
	struct jmsg *jm = NULL;
	char *name = NULL;

	fprintf(sig->ws, "\n{\"account\":\"%s\",\"address\":{\"number\":\"%s\"},\"type\":\"get_profile\",\"version\":\"v1\"}\n", sig->sender, phone);

	jm = jmsg_read_parse(sig);
	if (!jm) {
		fprintf(stderr, "sig_get_profile: error reading or parsing\n");
		goto out;
	}
	name = jmsg_lookup_string(jm, "data.name");

	printf("%s\n", jm->orig);
	printf("name: \"%s\"\n", name);

out:
	free(name);
	if (jm)
		jmsg_free(jm);
}

// TODO: return the list rather than printing it
void sig_list_groups(struct cbot_signal_backend *sig)
{
	struct jmsg *jm = NULL;
	size_t ix;
	char *title;
	fprintf(sig->ws, "\n{\"account\":\"%s\",\"type\":\"list_groups\",\"version\":\"v1\"}\n", sig->sender);

	jm = jmsg_read_parse(sig);
	if (!jm) {
		fprintf(stderr, "sig_get_profile: error reading or parsing\n");
		return;
	}
	printf("%s\n", jm->orig);

	ix = jmsg_lookup(jm, "data.groups");
	if (ix == 0) {
		printf("Key 'groups' not found in response: %s\n", jm->orig);
		jmsg_free(jm);
		return;
	}
	json_array_for_each(ix, jm->tok, ix)
	{
		title = jmsg_lookup_string_at(jm, ix, "title");
		if (!title) {
			printf("Failed to read title of group\n");
			break;
		}
		printf("Group: \"%s\"\n", title);
	}
	jmsg_free(jm);
}

void sig_subscribe(struct cbot_signal_backend *sig)
{
	char fmt[] = "\n{\"type\":\"subscribe\",\"username\":\"%s\"}\n";
	fprintf(sig->ws, fmt, sig->sender);
        sig_expect(sig, "subscribed");
}

void sig_set_name(struct cbot_signal_backend *sig, char *name)
{
	printf("SET {\"account\":\"%s\",\"name\":\"%s\",\"type\":\"set_profile\",\"version\":\"v1\"}\n", sig->sender, name);
	fprintf(sig->ws, "\n{\"account\":\"%s\",\"name\":\"%s\",\"type\":\"set_profile\",\"version\":\"v1\"}\n", sig->sender, name);
        sig_expect(sig, "set_profile");
}

void sig_expect(struct cbot_signal_backend *sig, const char *type)
{
	struct jmsg *jm;
	size_t ix_type;
	jm = jmsg_read_parse(sig);
	if (!jm) {
		fprintf(stderr, "sig_expect: error reading jmsg\n");
		return;
	}
	ix_type = json_object_get(jm->orig, jm->tok, 0, "type");
	if (ix_type == 0)
		fprintf(stderr, "sig_expect: no \"type\" field found in jmsg\n");
	else if (!json_string_match(jm->orig, jm->tok, ix_type, type))
		fprintf(stderr, "sig_expect: expected message type %s, but got something else\n", type);
	jmsg_free(jm);
}

const static char fmt_send_group[] = (
	"\n{"
	    "\"username\":\"%s\","
	    "\"recipientGroupId\":\"%s\","
	    "\"messageBody\":\"%s\","
	    "\"mentions\":[%s],"
	    "\"type\":\"send\","
	    "\"version\":\"v1\""
	"}\n"
);

void sig_send_group(struct cbot_signal_backend *sig, const char *to, const char *msg)
{
        char *quoted, *mentions = NULL;
        quoted = json_quote_and_mention(msg, &mentions);
        fprintf(sig->ws, fmt_send_group, sig->sender, to, quoted, mentions);
        free(quoted);
        free(mentions);
}

const static char fmt_send_single[] = (
	"\n{"
	    "\"username\":\"%s\","
	    "\"recipientAddress\":{"
	        "\"uuid\":\"%s\""
	    "},"
	    "\"messageBody\":\"%s\","
	    "\"mentions\":[%s],"
	    "\"type\":\"send\","
	    "\"version\":\"v1\""
	"}\n"
);

void sig_send_single(struct cbot_signal_backend *sig, const char *to, const char *msg)
{
        char *quoted, *mentions = NULL;
        quoted = json_quote_and_mention(msg, &mentions);
        fprintf(sig->ws, fmt_send_single, sig->sender, to, quoted, mentions);
        free(quoted);
        free(mentions);
}
