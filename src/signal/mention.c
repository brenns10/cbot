/*
 * Functions which help add and remove mentions to Signal messages.
 */
#include <stdlib.h>

#include <sc-collections.h>

#include "internal.h"

const char MENTION_PLACEHOLDER[] = {0xEF, 0xBF, 0xBC, 0x00};

char *mention_format(char *string, const char *prefix)
{
	struct sc_charbuf cb;
	sc_cb_init(&cb, 64);
	sc_cb_printf(&cb, "@(%s:%s)", prefix, string);
	free(string);
	return cb.buf;
}

/*
 * Checks for a prefix, if so, returns a pointer to directly after it.
 */
static const char *startswith(const char *str, const char *pfx)
{
	int len = strlen(pfx);
	if (strncmp(str, pfx, len) == 0)
		return str + len;
	return NULL;
}

char *mention_parse(const char *string, int *kind, int *offset)
{
	const char *start, *end;
	char *out;

	if ((start = startswith(string, "@(uuid:"))) {
		*kind = MENTION_USER;
	} else if ((start = startswith(string, "@(group:"))) {
		*kind = MENTION_GROUP;
	} else {
		*kind = MENTION_ERR;
		if (offset)
			*offset = 1;
		return strdup("@???");
	}
	end = strchr(start, ')');
	if (!end) {
		*kind = MENTION_ERR;
		if (offset)
			*offset = 1;
		return strdup("@???");
	}
	out = malloc(end - start + 1);
	memcpy(out, start, end - start);
	out[end - start] = '\0';
	if (offset)
		*offset = end - string + 1;
	return out;
}

char *mention_from_json(const char *str, struct jmsg *jm, size_t list)
{
	const char *next, *at;
	size_t uuid_idx;
	struct sc_charbuf cb;
	sc_cb_init(&cb, jm->origlen);

	if (list != 0)
		list = jm->tok[list].child;

	for (;;) {
		/* find next occurrence of either a mention or @ (which needs
		 * escaping) */
		next = strstr(str, MENTION_PLACEHOLDER);
		at = strchr(str, '@');

		/* Choose the first one, or if neither found, terminate loop */
		if (!next && !at)
			break;
		else if (next && at && (at < next))
			next = at;
		else if (!next)
			next = at;

		sc_cb_memcpy(&cb, str, next - str);

		/* Escape @ */
		if (*next == '@') {
			sc_cb_concat(&cb, "@@");
			str = next + 1;
			continue;
		}

		if (list == 0) {
			fprintf(stderr, "cbot signal: too few JSON mentions\n");
			sc_cb_concat(&cb, "???");
			str = next + sizeof(MENTION_PLACEHOLDER) - 1;
			continue;
		}
		sc_cb_concat(&cb, "@(uuid:");
		uuid_idx = jmsg_lookup_at(jm, list, "uuid");
		if (jm->tok[uuid_idx].type != JSON_STRING) {
			fprintf(stderr, "cbot signal: BADLY FORMATTED MENTION\n");
		} else {
			/* In general this isn't safe to manually access cb.buf, but
			 * in our case we preallocated it to be the size of the
			 * original JSON message, which means it must have space
			 * for a subset of it.
			 */
			json_string_load(jm->orig, jm->tok, uuid_idx, cb.buf + cb.length);
			cb.length += jm->tok[uuid_idx].length;
			cb.buf[cb.length] = '\0';
		}
		sc_cb_append(&cb, ')');
		list = jm->tok[list].next;
		str = next + sizeof(MENTION_PLACEHOLDER) - 1;
	}

	if (jm->tok[list].next != 0) {
		fprintf(stderr, "WARNING: unconsumed mention in JSON\n");
	}
	sc_cb_memcpy(&cb, str, strlen(str) + 1);
	return cb.buf;
}

char *json_quote_and_mention(const char *instr, char **mentions)
{
	size_t i = 0;
	struct sc_charbuf cb;
	struct sc_charbuf mb;

	sc_cb_init(&cb, strlen(instr));
	sc_cb_init(&mb, 128);
	for (i = 0; instr[i]; i++) {
		if (instr[i] == '"' || instr[i] == '\\') {
			sc_cb_append(&cb, '\\');
			sc_cb_append(&cb, instr[i]);
		} else if (instr[i] == '\n') {
			sc_cb_append(&cb, '\\');
			sc_cb_append(&cb, 'n');
		} else if (instr[i] == '@' && instr[i + 1] == '@') {
			sc_cb_append(&cb, '@');
			i++;
		} else if (instr[i] == '@' && instr[i + 1] != '@') {
			int kind, offset;
			char *mention;
			mention = mention_parse(instr + i, &kind, &offset);
			if (kind != MENTION_USER) {
				sc_cb_append(&cb, '@');
			} else {
				if (mb.length)
					sc_cb_append(&mb, ',');
				sc_cb_printf(&mb, "{\"length\": 0,\"start\": %d,\"uuid\":\"%s\"}", cb.length, mention);
				// Rather than use the MENTION_PLACEHOLDER, my
				// experiments indicate it's acceptable to
				// specify a zero-length placeholder
				i += offset - 1;
			}
			free(mention);
		} else {
			sc_cb_append(&cb, instr[i]);
		}
	}
	*mentions = mb.buf;
	return cb.buf;
}
