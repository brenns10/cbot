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
			fprintf(stderr,
			        "cbot_signal: pipe is empty, exiting\n");
			return -1;
		} else {
			return rv;
		}
	}
}

static int jmsg_parse(struct jmsg *jm)
{
	struct json_parser p = json_parse(jm->orig, NULL, 0);

	if (p.error != JSONERR_NO_ERROR) {
		fprintf(stderr, "json parse error:\n");
		json_print_error(stderr, p);
		return -1;
	}

	jm->tok = calloc(p.tokenidx, sizeof(*jm->tok));
	jm->toklen = p.tokenidx;
	p = json_parse(jm->orig, jm->tok, jm->toklen);
	assert(p.error == JSONERR_NO_ERROR);
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
	struct jmsg *jm;

	sc_cb_init(&cb, 4096);

	for (;;) {
		rv = async_read(fd, cb.buf + cb.length,
		                cb.capacity - cb.length);
		if (rv < 0) {
			fprintf(stderr, "read error: %d\n", rv);
			goto err;
		} else {
			cb.length += rv;
		}

		found = memchr(cb.buf, '\n', cb.length);
		while (found) {
			/*
			 * Find start of next message and replace newline
			 * with nul terminator
			 */
			nextmsgidx = found - cb.buf + 1;
			*found = '\0';
			printf("rv: %d nextmsgidx: %d length: %d\n", rv,
			       nextmsgidx, cb.length);

			/*
			 * Copy data into new jmsg and add to output.
			 */
			jm = calloc(1, sizeof(*jm));
			sc_list_init(&jm->list);
			jm->orig = malloc(nextmsgidx);
			memcpy(jm->orig, cb.buf, nextmsgidx);
			jm->origlen = nextmsgidx - 1;
			printf("JM: \"%s\"\n", jm->orig);
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

void jmsg_free(struct jmsg *jm)
{
	if (jm) {
		free(jm->orig);
		free(jm->tok);
		free(jm);
	}
}

char *jmsg_lookup_string_at_len(struct jmsg *jm, size_t start, const char *key,
                                size_t *len)
{
	size_t res = jmsg_lookup_at(jm, start, key);
	char *str;
	if (res == 0)
		return NULL;
	if (jm->tok[res].type != JSON_STRING)
		return NULL;
	if (len)
		*len = jm->tok[res].length;
	str = malloc(jm->tok[res].length + 1);
	json_string_load(jm->orig, jm->tok, res, str);
	return str;
}
