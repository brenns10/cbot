/*
 * Functions which help add and remove mentions to Signal messages.
 */
#include <assert.h>
#include <sc-collections.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbot/cbot.h"
#include "internal.h"
#include "nosj.h"

const char MENTION_PLACEHOLDER[] = { 0xEF, 0xBF, 0xBC, 0x00 };

char *mention_format_p(char *string, const char *prefix)
{
	struct sc_charbuf cb;
	sc_cb_init(&cb, 64);
	sc_cb_printf(&cb, "@(%s:%s)", prefix, string);
	return cb.buf;
}

char *mention_format(char *string, const char *prefix)
{
	char *res = mention_format_p(string, prefix);
	free(string);
	return res;
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

char *mention_from_json(const char *str, struct jmsg *jm, uint32_t list)
{
	const char *next, *at;
	uint32_t uuid_idx;
	struct sc_charbuf cb;
	sc_cb_init(&cb, jm->easy.input_len);

	/* List starts at index +1 */
	list++;
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
			CL_CRIT("cbot signal: too few JSON mentions\n");
			sc_cb_concat(&cb, "???");
			str = next + sizeof(MENTION_PLACEHOLDER) - 1;
			continue;
		}
		if (jmsg_lookup_at(jm, list, "uuid", &uuid_idx) != JSON_OK) {
			CL_CRIT("cbot signal: can't find UUID in JSON "
			        "mention\n");
			sc_cb_concat(&cb, "???");
			str = next + sizeof(MENTION_PLACEHOLDER) - 1;
			/* We should skip this item in the list, it's malformed
			 */
			list = jm->easy.tokens[list].next;
			continue;
		}
		sc_cb_concat(&cb, "@(uuid:");
		if (jm->easy.tokens[uuid_idx].type != JSON_STRING) {
			CL_CRIT("cbot signal: BADLY FORMATTED MENTION\n");
		} else {
			/* In general this isn't safe to manually access cb.buf,
			 * but in our case we preallocated it to be the size of
			 * the original JSON message, which means it must have
			 * space for a subset of it.
			 */
			int ret = json_easy_string_load(&jm->easy, uuid_idx,
			                                cb.buf + cb.length);
			assert(ret == JSON_OK); /* We already type checked */
			cb.length += jm->easy.tokens[uuid_idx].length;
			cb.buf[cb.length] = '\0';
		}
		sc_cb_append(&cb, ')');
		list = jm->easy.tokens[list].next;
		str = next + sizeof(MENTION_PLACEHOLDER) - 1;
	}

	if (jm->easy.tokens[list].next != 0) {
		CL_WARN("unconsumed mention in JSON\n");
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
				sc_cb_printf(&mb,
				             "{\"length\": 0,\"start\": "
				             "%d,\"uuid\":\"%s\"}",
				             cb.length, mention);
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
