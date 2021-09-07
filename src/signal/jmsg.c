/*
 * Functions to read a JSON message and provide it and an API to access data
 * from it.
 */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <sc-lwt.h>

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
			fprintf(stderr,
			        "cbot_signal: pipe is empty, exiting\n");
			return -1;
		} else {
			return rv;
		}
	}
}

struct jmsg *jmsg_read(struct cbot_signal_backend *sig)
{
	int rv, foundidx;
	struct sc_charbuf cb;
	char *found;
	size_t ls = 0;
	struct jmsg *jm;

	sc_cb_init(&cb, 4096);

	/* Copy any spilled data into bufs->orig */
	if (sig->spilllen) {
		sc_cb_memcpy(&cb, sig->spill, sig->spilllen);
		sig->spilllen = 0;
	}
	for (;;) {
		/*
		 * Search for the terminator. This is at the top of the loop so
		 * that any additional terminators in the spill buffer get
		 * found before reading more data.
		 */
		found = memchr(cb.buf + ls, '\n', cb.length - ls);
		ls = cb.length;
		if (found) {
			/*
			 * Copy everything after the terminator to the spill
			 * buffer, NUL terminate and return.
			 */
			foundidx = found - cb.buf + 1;
			if (foundidx != cb.length) {
				sig->spilllen = cb.length - foundidx;
				memcpy(sig->spill, found + 1, sig->spilllen);
			}
			*found = '\0';
			jm = calloc(1, sizeof(*jm));
			jm->orig = cb.buf;
			jm->origlen = foundidx - 1;
			return jm;
		}

		/* Ensure there is space to read more data */
		if (cb.length == cb.capacity) {
			cb.capacity *= 2;
			cb.buf = realloc(cb.buf, cb.capacity);
		}

		/* Read data */
		rv = async_read(sig->fd, cb.buf + cb.length,
		                cb.capacity - cb.length);
		if (rv < 0) {
			sc_cb_destroy(&cb);
			fprintf(stderr, "read error: %d\n", rv);
			return NULL;
		} else {
			cb.length += rv;
		}
	}
}

int jmsg_parse(struct jmsg *jm)
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

struct jmsg *jmsg_read_parse(struct cbot_signal_backend *sig)
{
	struct jmsg *jm;
	jm = jmsg_read(sig);
	if (!jm || jmsg_parse(jm) != 0) {
		fprintf(stderr, "sig_get_profile: error reading or parsing\n");
		return NULL;
	}
	return jm;
}

void jmsg_free(struct jmsg *jm)
{
	free(jm->orig);
	if (jm->tok)
		free(jm->tok);
	free(jm);
}

char *jmsg_lookup_string_at_len(struct jmsg *jm, size_t start, const char *key, size_t *len)
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
