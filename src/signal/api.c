/*
 * Signal(d) API functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "nosj.h"
#include "sc-collections.h"

void sig_user_free(struct signal_user *user)
{
	if (user) {
		free(user->first_name);
		free(user->number);
		free(user->uuid);
		free(user);
	}
}

void sig_user_free_all(struct sc_list_head *list)
{
	struct signal_user *user, *next;
	sc_list_for_each_safe(user, next, list, list, struct signal_user)
	{
		sc_list_remove(&user->list);
		sig_user_free(user);
	}
}

static struct signal_user *__sig_parse_profile(struct jmsg *jm, size_t ix)
{
	struct signal_user *user;
	char *nul;
	size_t namelen;

	user = malloc(sizeof(*user));
	sc_list_init(&user->list);
	user->first_name = jmsg_lookup_string_at_len(jm, ix, "name", &namelen);
	if (user->first_name) {
		nul = strchr(user->first_name, '\0');
		if ((nul - user->first_name) < namelen)
			user->last_name = nul + 1;
		else
			user->last_name = NULL;
	}
	user->number = jmsg_lookup_string_at(jm, ix, "address.number");
	user->uuid = jmsg_lookup_string_at(jm, ix, "address.uuid");
	return user;
}

static struct signal_user *__sig_get_profile(struct cbot_signal_backend *sig,
                                             const char *ident,
                                             const char *kind)
{
	struct jmsg *jm = NULL;
	struct signal_user *user;

	fprintf(sig->ws,
	        "\n{\"account\":\"%s\",\"address\":{\"%s\":\"%s\"},\"type\":"
	        "\"get_profile\",\"version\":\"v1\"}\n",
	        sig->sender, kind, ident);

	jm = jmsg_next(sig);
	if (!jm) {
		fprintf(stderr, "sig_get_profile: error reading or parsing\n");
		return NULL;
	}
	user = __sig_parse_profile(jm, jmsg_lookup(jm, "data"));
	jmsg_free(jm);
	return user;
}

struct signal_user *sig_get_profile(struct cbot_signal_backend *sig,
                                    const char *phone)
{
	return __sig_get_profile(sig, phone, "number");
}

struct signal_user *sig_get_profile_by_uuid(struct cbot_signal_backend *sig,
                                            const char *uuid)
{
	return __sig_get_profile(sig, uuid, "uuid");
}

void sig_list_contacts(struct cbot_signal_backend *sig,
                       struct sc_list_head *head)
{
	struct jmsg *jm;
	struct signal_user *user;
	size_t ix;
	fprintf(sig->ws,
	        "\n{\"account\":\"%s\",\"type\":\"list_contacts\",\"version\":"
	        "\"v1\"}\n",
	        sig->sender);
	jm = jmsg_next(sig);
	ix = jmsg_lookup(jm, "data.profiles");
	json_array_for_each(ix, jm->tok, ix)
	{
		user = __sig_parse_profile(jm, ix);
		sc_list_insert_end(head, &user->list);
	}
}

void sig_group_free(struct signal_group *grp)
{
	size_t i;
	if (grp) {
		free(grp->id);
		free(grp->title);
		free(grp->invite_link);
		for (i = 0; i < grp->n_members; i++)
			free(grp->members[i].uuid);
		free(grp->members);
		free(grp);
	}
}

void sig_group_free_all(struct sc_list_head *list)
{
	struct signal_group *grp, *next;
	sc_list_for_each_safe(grp, next, list, list, struct signal_group)
	{
		sc_list_remove(&grp->list);
		sig_group_free(grp);
	}
}

int sig_list_groups(struct cbot_signal_backend *sig, struct sc_list_head *list)
{
	struct jmsg *jm = NULL;
	size_t ix, jx, kx;
	int count = 0;
	struct signal_group *group;
	struct signal_member *memb;
	fprintf(sig->ws,
	        "\n{\"account\":\"%s\",\"type\":\"list_groups\",\"version\":"
	        "\"v1\"}\n",
	        sig->sender);

	jm = jmsg_next(sig);
	if (!jm) {
		fprintf(stderr, "sig_get_profile: error reading or parsing\n");
		return -1;
	}

	ix = jmsg_lookup(jm, "data.groups");
	if (ix == 0) {
		printf("Key 'groups' not found in response: %s\n", jm->orig);
		jmsg_free(jm);
		return -1;
	}
	json_array_for_each(ix, jm->tok, ix)
	{
		group = malloc(sizeof(*group));
		sc_list_init(&group->list);
		group->id = jmsg_lookup_string_at(jm, ix, "id");
		group->title = jmsg_lookup_string_at(jm, ix, "title");
		group->invite_link =
		        jmsg_lookup_string_at(jm, ix, "inviteLink");
		jx = jmsg_lookup_at(jm, ix, "memberDetail");
		group->members = calloc(jm->tok[jx].length,
		                        sizeof(struct signal_member));
		group->n_members = 0;
		json_array_for_each(jx, jm->tok, jx)
		{
			memb = &group->members[group->n_members++];
			memb->uuid = jmsg_lookup_string_at(jm, jx, "uuid");
			kx = jmsg_lookup_at(jm, jx, "role");
			if (json_string_match(jm->orig, jm->tok, kx,
			                      "ADMINISTRATOR"))
				memb->role = SIGNAL_ROLE_ADMINISTRATOR;
			else
				memb->role = SIGNAL_ROLE_DEFAULT;
		}
		sc_list_insert_end(list, &group->list);
		count++;
	}
	jmsg_free(jm);
	return count;
}

void sig_subscribe(struct cbot_signal_backend *sig)
{
	char fmt[] = "\n{\"type\":\"subscribe\",\"username\":\"%s\"}\n";
	fprintf(sig->ws, fmt, sig->sender);
	sig_expect(sig, "subscribed");
}

void sig_set_name(struct cbot_signal_backend *sig, const char *name)
{
	printf("SET "
	       "{\"account\":\"%s\",\"name\":\"%s\",\"type\":\"set_profile\","
	       "\"version\":\"v1\"}\n",
	       sig->sender, name);
	fprintf(sig->ws,
	        "\n{\"account\":\"%s\",\"name\":\"%s\",\"type\":\"set_"
	        "profile\",\"version\":\"v1\"}\n",
	        sig->sender, name);
	sig_expect(sig, "set_profile");
}

void sig_expect(struct cbot_signal_backend *sig, const char *type)
{
	struct jmsg *jm;
	size_t ix_type;
	bool found = false;

	/*
	 * TODO: this drops unrelated messages which is... dumb
	 * Need to redesign JMSG reading so that we queue up JMSGs for
	 * processing, then place all unrelated JMSGs on that queue here.
	 */
	while (!found) {
		jm = jmsg_next(sig);
		if (!jm) {
			fprintf(stderr, "sig_expect: error reading jmsg\n");
			jmsg_free(jm);
			return;
		}
		ix_type = json_object_get(jm->orig, jm->tok, 0, "type");
		if (ix_type == 0) {
			fprintf(stderr, "sig_expect: no \"type\" field found "
			                "in jmsg\n");
		} else if (!json_string_match(jm->orig, jm->tok, ix_type,
		                              type)) {
			fprintf(stderr,
			        "sig_expect: expected message type %s, but got "
			        "something else, waiting still...\n",
			        type);
		} else {
			printf("sig_expect: got message type %s\n", type);
			found = true;
		}
		jmsg_free(jm);
	}
}

const static char fmt_send_group[] = ("\n{"
                                      "\"username\":\"%s\","
                                      "\"recipientGroupId\":\"%s\","
                                      "\"messageBody\":\"%s\","
                                      "\"mentions\":[%s],"
                                      "\"type\":\"send\","
                                      "\"version\":\"v1\""
                                      "}\n");

void sig_send_group(struct cbot_signal_backend *sig, const char *to,
                    const char *msg)
{
	char *quoted, *mentions = NULL;
	quoted = json_quote_and_mention(msg, &mentions);
	fprintf(sig->ws, fmt_send_group, sig->sender, to, quoted, mentions);
	free(quoted);
	free(mentions);
}

const static char fmt_send_single[] = ("\n{"
                                       "\"username\":\"%s\","
                                       "\"recipientAddress\":{"
                                       "\"uuid\":\"%s\""
                                       "},"
                                       "\"messageBody\":\"%s\","
                                       "\"mentions\":[%s],"
                                       "\"type\":\"send\","
                                       "\"version\":\"v1\""
                                       "}\n");

void sig_send_single(struct cbot_signal_backend *sig, const char *to,
                     const char *msg)
{
	char *quoted, *mentions = NULL;
	quoted = json_quote_and_mention(msg, &mentions);
	fprintf(sig->ws, fmt_send_single, sig->sender, to, quoted, mentions);
	free(quoted);
	free(mentions);
}

char *__sig_resolve_address(struct cbot_signal_backend *sig, const char *kind,
                            const char *val)
{
	struct jmsg *jm;
	char *ret;
	fprintf(sig->ws,
	        "\n{\"account\":\"%s\",\"type\":\"resolve_address\","
	        "\"version\":\"v1\",\"partial\":{\"%s\":\"%s\"}}\n",
	        sig->sender, kind, val);

	jm = jmsg_next(sig);
	printf("%s\n", jm->orig);
	if (!jm) {
		fprintf(stderr,
		        "sig_resolve_address: error reading or parsing\n");
		return NULL;
	}
	if (strcmp(kind, "number") == 0)
		ret = jmsg_lookup_string(jm, "data.uuid");
	else
		ret = jmsg_lookup_string(jm, "data.number");
	jmsg_free(jm);
	return ret;
}

char *sig_get_number(struct cbot_signal_backend *sig, const char *uuid)
{
	return __sig_resolve_address(sig, "uuid", uuid);
}

char *sig_get_uuid(struct cbot_signal_backend *sig, const char *number)
{
	return __sig_resolve_address(sig, "number", number);
}
