/*
 * Functions to read a JSON message and provide it and an API to access data
 * from it.
 */
#include <assert.h>
#include <errno.h>
#include <sc-lwt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cbot/cbot.h"
#include "internal.h"
#include "nosj.h"
#include "sc-collections.h"

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

static struct jmsg *jmsg_find_type(struct sc_list_head *list, const char *type)
{
	struct jmsg *jm;
	uint32_t ix_type;

	sc_list_for_each_entry(jm, list, list, struct jmsg)
	{
		int ret;
		bool match;

		ret = json_easy_object_get(&jm->easy, 0, "type", &ix_type);
		if (ret != JSON_OK)
			continue;

		ret = json_easy_string_match(&jm->easy, ix_type, type, &match);
		if (ret != JSON_OK)
			continue;

		if (match) {
			sc_list_remove(&jm->list);
			return jm;
		}
	}

	return NULL;
}

struct jmsg *jmsg_wait(struct cbot_signal_backend *sig, const char *type)
{
	struct sc_list_head list;
	struct jmsg *jm = jmsg_find_type(&sig->messages, type);
	if (jm)
		return jm;

	for (;;) {
		sc_list_init(&list);
		if (jmsg_read(sig->fd, &list) < 0) {
			/* make sure we don't leak them */
			sc_list_move(&list, sig->messages.prev);
			return NULL;
		}
		jm = jmsg_find_type(&list, type);
		sc_list_move(&list, sig->messages.prev);
		if (jm)
			return jm;
	}
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

char *jmsg_lookup_string_at_len(struct jmsg *jm, uint32_t start,
                                const char *key, size_t *len)
{
	char *data;
	uint32_t idx;

	if (json_easy_lookup(&jm->easy, start, key, &idx) != 0)
		return NULL;
	if (json_easy_string_get(&jm->easy, idx, &data) != 0)
		return NULL;
	if (len)
		*len = jm->easy.tokens[idx].length;
	return data;
}
