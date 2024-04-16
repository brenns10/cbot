/*
 * signal/jmsg.c: utilities for reading and parsing lines of JSON data
 */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nosj.h>
#include <sc-collections.h>
#include <sc-lwt.h>

#include "../cbot_private.h"
#include "cbot/cbot.h"
#include "internal.h"

static int async_read(int fd, char *data, size_t nbytes)
{
	struct sc_lwt *cur = sc_lwt_current();
	int rv;

	for (;;) {
		rv = read(fd, data, nbytes);
		if (rv < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
			/* would block, we should yield */
			sc_lwt_set_state(cur, SC_LWT_BLOCKED);
			sc_lwt_yield();
		} else if (rv < 0) {
			perror("cbot_signal pipe read");
			return -1;
		} else if (rv == 0) {
			CL_CRIT("cbot_signal: pipe is empty, exiting\n");
			return -1;
		} else {
			return rv;
		}
	}
}

static int jmsg_parse(struct jmsg *jm)
{
	int res = json_easy_parse(&jm->easy);

	if (res != JSON_OK) {
		CL_CRIT("json parse error: %s\n", json_strerror(res));
		return -1;
	}
	return 0;
}

/*
 * Read at least one jmsg, adding it to the list. All jmsg are parsed.
 *
 * Return the number of successfully read jmsgs. On error, return -1 (though
 * successful messages may still be in the list).
 */
static int jmsg_read(int fd, struct sc_list_head *list)
{
	int rv, nextmsgidx;
	struct sc_charbuf cb;
	char *found;
	int count = 0;

	sc_cb_init(&cb, 4096);

	for (;;) {
		rv = async_read(fd, cb.buf + cb.length,
		                cb.capacity - cb.length);
		if (rv < 0) {
			CL_CRIT("read error: %d\n", rv);
			goto err;
		} else {
			cb.length += rv;
		}

		found = memchr(cb.buf, '\n', cb.length);
		while (found) {
			char *buf = NULL;
			struct jmsg *jm = NULL;

			/*
			 * Find start of next message and replace newline
			 * with nul terminator
			 */
			nextmsgidx = found - cb.buf + 1;
			*found = '\0';

			/*
			 * Copy data into new jmsg and add to output.
			 */
			buf = malloc(nextmsgidx);
			if (!buf) {
				CL_CRIT("Allocation error\n");
				goto err;
			}
			memcpy(buf, cb.buf, nextmsgidx);
			jm = calloc(1, sizeof(*jm));
			if (!jm) {
				CL_CRIT("Allocation error\n");
				free(buf);
				goto err;
			}
			json_easy_init(&jm->easy, buf);
			/* Weirdly, clang-tidy believes that here, buf could be
			 * leaked. I guess it doesn't pick up on the fact that
			 * now, jm->easy takes ownership of buf. Suppress the
			 * false positive.*/
			sc_list_init(&jm->list); // NOLINT
			CL_VERB("JM: \"%s\"\n", jm->easy.input);
			if (jmsg_parse(jm) < 0) {
				jmsg_free(jm);
				goto err;
			}
			sc_list_insert_end(list, &jm->list);
			count += 1;

			/*
			 * Skip past any possible additional newlines.
			 */
			while (nextmsgidx < cb.length &&
			       cb.buf[nextmsgidx] == '\n')
				nextmsgidx++;

			/*
			 * If there is no more data in the buffer, we're good.
			 * Return.
			 */
			if (nextmsgidx == cb.length) {
				sc_cb_destroy(&cb);
				return count;
			}

			/*
			 * Otherwise, shift data down to
			 */
			memmove(cb.buf, &cb.buf[nextmsgidx],
			        cb.length - nextmsgidx);
			cb.length -= nextmsgidx;
			found = memchr(cb.buf, '\n', cb.length);
		}

		/* Ensure there is space to read more data */
		if (cb.length == cb.capacity) {
			cb.capacity *= 2;
			cb.buf = realloc(cb.buf, cb.capacity);
		}
	}
err:
	sc_cb_destroy(&cb);
	return -1;
}

static struct jmsg *jmsg_first(struct sc_list_head *list)
{
	struct jmsg *jm;

	sc_list_for_each_entry(jm, list, list, struct jmsg)
	{
		sc_list_remove(&jm->list);
		return jm;
	}
	return NULL;
}

struct jmsg *jmsg_next(struct cbot_signal_backend *sig)
{
	struct jmsg *jm;

	if ((jm = jmsg_first(&sig->messages)))
		return jm;
	if (jmsg_read(sig->fd, &sig->messages) < 0)
		return NULL; /* need to propagate error */
	return jmsg_first(&sig->messages);
}

static struct jmsg *jmsg_find_by_field(struct sc_list_head *list,
                                       const char *field, const char *value)
{
	struct jmsg *jm;
	uint32_t ix_type;

	sc_list_for_each_entry(jm, list, list, struct jmsg)
	{
		int ret;
		bool match;

		ret = json_easy_object_get(&jm->easy, 0, field, &ix_type);
		if (ret != JSON_OK)
			continue;

		ret = json_easy_string_match(&jm->easy, ix_type, value, &match);
		if (ret != JSON_OK)
			continue;

		if (match) {
			sc_list_remove(&jm->list);
			return jm;
		}
	}

	return NULL;
}

struct signal_queued_item {
	struct sc_list_head list;
	const char *field;
	const char *value;
	struct sc_lwt *thread;
	struct jmsg *result;
};

struct jmsg *jmsg_wait_field(struct cbot_signal_backend *sig, const char *field,
                             const char *value)
{
	struct sc_list_head list;
	struct sc_lwt *cur;
	struct jmsg *jm = jmsg_find_by_field(&sig->messages, field, value);
	if (jm)
		return jm;

	cur = sc_lwt_current();
	if (cur != sig->bot->lwt) {
		struct signal_queued_item item = { 0 };
		item.field = field;
		item.value = value;
		item.thread = cur;
		sc_list_init(&item.list);
		sc_list_insert_end(&sig->msgq, &item.list);
		while (!item.result && !sc_lwt_shutting_down()) {
			sc_lwt_set_state(cur, SC_LWT_BLOCKED);
			sc_lwt_set_state(sig->bot->lwt, SC_LWT_RUNNABLE);
			sc_lwt_yield();
		}
		return item.result;
	}

	for (;;) {
		sc_list_init(&list);
		if (jmsg_read(sig->fd, &list) < 0) {
			/* make sure we don't leak them */
			sc_list_move(&list, sig->messages.prev);
			return NULL;
		}
		jm = jmsg_find_by_field(&list, field, value);
		sc_list_move(&list, sig->messages.prev);
		if (jm)
			return jm;
	}
}

bool jmsg_deliver(struct cbot_signal_backend *sig, struct jmsg *jm)
{
	struct signal_queued_item *item;
	sc_list_for_each_entry(item, &sig->msgq, list,
	                       struct signal_queued_item)
	{
		if (je_string_match(&jm->easy, 0, item->field, item->value)) {
			sc_list_remove(&item->list);
			item->result = jm;
			sc_lwt_set_state(item->thread, SC_LWT_RUNNABLE);
			return true;
		}
	}
	return false;
}

void jmsg_free(struct jmsg *jm)
{
	if (jm) {
		/* json_easy does not own input */
		free((void *)jm->easy.input);
		json_easy_destroy(&jm->easy);
		free(jm);
	}
}

int je_get_object(struct json_easy *je, uint32_t start, const char *key,
                  uint32_t *out)
{
	int err = json_easy_lookup(je, start, key, out);
	if (err != JSON_OK)
		return err;
	if (je->tokens[*out].type != JSON_OBJECT)
		return JSONERR_TYPE;
	return JSON_OK;
}
int je_get_array(struct json_easy *je, uint32_t start, const char *key,
                 uint32_t *out)
{
	int err = json_easy_lookup(je, start, key, out);
	if (err != JSON_OK)
		return err;
	if (je->tokens[*out].type != JSON_ARRAY)
		return JSONERR_TYPE;
	return JSON_OK;
}
int je_get_uint(struct json_easy *je, uint32_t start, const char *key,
                uint64_t *out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	return json_easy_number_getuint(je, index, out);
}
int je_get_int(struct json_easy *je, uint32_t start, const char *key,
               int64_t *out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	return json_easy_number_getint(je, index, out);
}
int je_get_bool(struct json_easy *je, uint32_t start, const char *key,
                bool *out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	*out = je->tokens[index].type == JSON_TRUE;
	return JSON_OK;
}
int je_get_string(struct json_easy *je, uint32_t start, const char *key,
                  char **out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	return json_easy_string_get(je, index, out);
}

bool je_string_match(struct json_easy *je, uint32_t start, const char *key,
                     const char *cmp)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return false;
	bool match;
	if (json_easy_string_match(je, index, cmp, &match) != JSON_OK)
		return false;
	return match;
}
