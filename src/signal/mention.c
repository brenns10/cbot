/*
 * Functions which help add and remove mentions to Signal messages.
 */
#include <sc-collections.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utf8.h"
#include "cbot/cbot.h"
#include "internal.h"
#include "nosj.h"

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

/*
 * What is this monstrosity of a function? Why does it mention UTF-8?
 *
 * Each mention is supposed to be a sub-string, but what unit are the substrings
 * measured in terms of? Is it Unicode code points? That might make sense to
 * _normal_ developers, but not to Signal developers.
 *
 * Well maybe the substring is measured in terms of UTF-8 encoded bytes? After
 * all, UTF-8 is the de-facto standard for almost all text nowadays.
 *
 * But no! Signal seems to use the unit of "UTF-16 code units". Which is
 * *almost* the same as Unicode code points, except that emojis and other
 * characters beyond the BMP are represented using TWO UTF-16 code units.
 * This function exists to "advance" a byte-index forward to a certan UTF-16
 * code unit index, so that we can accurately identify substrings for
 * replacement.
 */
int index_of_utf16(const char *str, int index, int u16units, int end_u16,
                   int len)
{
	while (index < len) {
		int nbytes = utf8_nbytes(str[index]);
		index += nbytes;

		/* 4-byte UTF-8 representation is beyond the BMP. All code
		 * points represented by 4 bytes in UTF-8 require surrogate
		 * pairs in UTF-16. */
		u16units += (nbytes == 4) ? 2 : 1;

		if (u16units == end_u16) {
			return index;
		} else if (u16units == end_u16) {
			/* BUG: can't split a surrogate pair for @mention */
			CL_CRIT("attempted to split surrogate pair for "
			        "@mention");
			return index;
		}
	}
	CL_CRIT("indexed past end of string for @mention");
	return index;
}

/* Copy text into the buffer, duplicating (escaping) any @ sign */
static void copy_in(struct sc_charbuf *cb, const char *str, int start, int end)
{
	for (int i = start; i < end; i++) {
		sc_cb_append(cb, str[i]);
		if (str[i] == '@')
			sc_cb_append(cb, '@');
	}
}

char *mention_from_json(const char *str, struct json_easy *je, uint32_t list)
{
	int bytes = 0, u16units = 0;
	int len = strlen(str);
	uint32_t elem;
	struct sc_charbuf cb;
	int err;
	sc_cb_init(&cb, je->input_len);

	json_easy_for_each(elem, je, list)
	{
		uint64_t start, length;
		char *uuid;

		if ((err = je_get_uint(je, elem, "start", &start)) != JSON_OK) {
			CL_CRIT("mention_from_json: failed to load \"start\": "
			        "%s\n",
			        json_strerror(err));
			goto err;
		}
		if ((err = je_get_uint(je, elem, "length", &length)) !=
		    JSON_OK) {
			CL_CRIT("mention_from_json: failed to load \"length\": "
			        "%s\n",
			        json_strerror(err));
			goto err;
		}
		if ((err = je_get_string(je, elem, "uuid", &uuid)) != JSON_OK) {
			CL_CRIT("mention_from_json: failed to load \"uuid\": "
			        "%s\n",
			        json_strerror(err));
			goto err;
		}

		/* Copy message up to the mention into the buffer */
		if (start > u16units) {
			int new_bytes = index_of_utf16(str, bytes, u16units,
			                               start, len);
			copy_in(&cb, str, bytes, new_bytes);
			bytes = new_bytes;
			u16units = start;
		}

		/* Now append a UUID mention */
		sc_cb_printf(&cb, "@(uuid:%s)", uuid);
		free(uuid);

		if (length) {
			/* Now skip over the part of the string specified */
			int new_bytes = index_of_utf16(str, bytes, u16units,
			                               u16units + length, len);
			bytes = new_bytes;
			u16units += length;
		}
	}

	copy_in(&cb, str, bytes, len);
	sc_cb_trim(&cb);
	return cb.buf;
err:
	sc_cb_destroy(&cb);
	return NULL;
}

char *json_quote_and_mention(const char *instr, char **mentions)
{
	size_t i = 0;
	struct sc_charbuf cb;
	struct sc_charbuf mb;

	// XXX need to fix the start index to use UTF-16 code units

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
