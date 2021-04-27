#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <wait.h>
#include <assert.h>

#include <nosj.h>

#include "cbot_private.h"

struct cbot_signal_backend {

	/* The Unix domain socket connecting us to Signald */
	int fd;

	/* Any data from the last read which hasn't yet been processed */
	char *spill;
	size_t spilllen;

	/*
	 * A stdio write stream associated with the above socket. It is in
	 * unbuffered mode, used to write formatted JSON commands.
	 */
	FILE *ws;

	/* Phone number of the bot sender */
	char *sender;

	/* Reference to the bot */
	struct cbot *bot;
};

/*
 * Structure representing a line of text which is a JSON message.
 * Owns the orig and tok pointers (though tok may be null).
 * Can be parsed, and then subsequent lookup operations can happen.
 */
struct jmsg {
	char *orig;
	size_t origlen;

	struct json_token *tok;
	size_t toklen;
};

/* I/O functions for interacting with sockets and creating jmsg objects */

/**
 * Wrap the read() system call for the sc_lwt async use case.
 */
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

/**
 * Read into bufs->orig, until we hit a character c. This character is assumed
 * to be a message terminator, and is replaced with a null terminator. The
 * number of bytes read (including that character c which became the NUL
 * terminator) is returned. Any extra data which came from the final read (after
 * c) is copied into the spill buffer, and the spill buffer is drained before
 * doing any further reading.
 *
 * In other words, a very basic buffered I/O system built on the async_read()
 * primitive.
 */
static struct jmsg *sig_read_jmsg(struct cbot_signal_backend *sig)
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

static struct jmsg *sig_read_parse_jmsg(struct cbot_signal_backend *sig)
{
	struct jmsg *jm;
	jm = sig_read_jmsg(sig);
	if (!jm || jmsg_parse(jm) != 0) {
		fprintf(stderr, "sig_get_profile: error reading or parsing\n");
		return NULL;
	}
	return jm;
}

static void jmsg_free(struct jmsg *jm)
{
	free(jm->orig);
	if (jm->tok)
		free(jm->tok);
	free(jm);
}


static char *jmsg_lookup_stringnul(struct jmsg *jm, char *key, char val)
{
	size_t res = json_lookup(jm->orig, jm->tok, 0, key);
	char *str, *f;
	if (res == 0)
		return NULL;
	if (jm->tok[res].type != JSON_STRING)
		return NULL;
	str = malloc(jm->tok[res].length + 1);
	json_string_load(jm->orig, jm->tok, res, str);
	if (val) {
		f = str;
		do {
			f = strchr(f, '\0');
			if (!f || (f - str) >= jm->tok[res].length)
				break;
			*f = val;
		} while (1);
	}
	return str;
}

static char *jmsg_lookup_string(struct jmsg *jm, char *key)
{
	return jmsg_lookup_stringnul(jm, key, '\0');
}

/* Utility functions */

/*
 * Adds a prefix to the beginning of a string. Uses realloc(), so beware.
 */
static char *realloc_with_prefix(char *string, const char *prefix)
{
	int str_len = strlen(string);
	int prefix_len = strlen(prefix);
	int len = str_len + prefix_len + 1;
	string = realloc(string, len);
	memmove(string + prefix_len, string, str_len + 1);
	memcpy(string, prefix, prefix_len);
	return string;
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

/*
 * Return a newly allocated string with necessary escaping for JSON.
 */
static char *json_quote(const char *instr)
{
	size_t i, j;
	int additional = 0;
	char *outstr = NULL;
	for (i = 0; instr[i]; i++)
		if (instr[i] == '"' || instr[i] == '\n' || instr[i] == '\\')
			additional++;

	outstr = malloc(i + additional);
	for (i = 0, j = 0; instr[i]; i++) {
		if (instr[i] == '"' || instr[i] == '\\') {
			outstr[j++] = '\\';
			outstr[j++] = instr[i];
		} else if (instr[i] == '\n') {
			outstr[j++] = '\\';
			outstr[j++] = 'n';
		} else {
			outstr[j++] = instr[i];
		}
	}
	return outstr;
}

/* Signald / Signal API functions */

static void sig_get_profile(struct cbot_signal_backend *sig, char *phone)
{
	struct jmsg *jm = NULL;
	char *name = NULL;

	fprintf(sig->ws, "\n{\"account\":\"%s\",\"address\":{\"number\":\"%s\"},\"type\":\"get_profile\",\"version\":\"v1\"}\n", sig->sender, phone);

	jm = sig_read_parse_jmsg(sig);
	if (!jm != 0) {
		fprintf(stderr, "sig_get_profile: error reading or parsing\n");
		goto out;
	}
	name = jmsg_lookup_stringnul(jm, "data.name", ' ');

	printf("%s\n", jm->orig);
	printf("name: \"%s\"\n", name);

out:
	free(name);
	if (jm)
		jmsg_free(jm);
}

static void sig_subscribe(struct cbot_signal_backend *sig)
{
	char fmt[] = "\n{\"type\":\"subscribe\",\"username\":\"%s\"}\n";
	fprintf(sig->ws, fmt, sig->sender);

}

static void sig_set_name(struct cbot_signal_backend *sig, char *name)
{
	printf("SET {\"account\":\"%s\",\"name\":\"%s\",\"type\":\"set_profile\",\"version\":\"v1\"}\n", sig->sender, name);
	fprintf(sig->ws, "\n{\"account\":\"%s\",\"name\":\"%s\",\"type\":\"set_profile\",\"version\":\"v1\"}\n", sig->sender, name);
}

static void sig_expect(struct cbot_signal_backend *sig, const char *type)
{
	struct jmsg *jm;
	size_t ix_type;
	jm = sig_read_parse_jmsg(sig);
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

/* Backend function implementations */

static int cbot_signal_configure(struct cbot *bot, config_setting_t *group)
{
	struct cbot_signal_backend *backend;
	struct sockaddr_un addr;
	int rv;
	const char *phone;

	rv = config_setting_lookup_string(group, "phone", &phone);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr, "cbot signal: key \"phone\" wrong type or not exists\n");
		return -1;
	}

	backend = calloc(1, sizeof(*backend));
	backend->sender = strdup(phone);
	backend->spill = malloc(4096);
	backend->spilllen = 0;

	backend->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (backend->fd < 0) {
		perror("create socket");
		goto out1;
	}
	backend->ws = fdopen(backend->fd, "w");
	if (!backend->ws) {
		perror("fdopen socket");
		goto out2;
	}
	setvbuf(backend->ws, NULL, _IONBF, 0);

	addr = (struct sockaddr_un){ AF_UNIX, "/var/run/signald/signald.sock" };
	rv = connect(backend->fd, (struct sockaddr *)&addr, sizeof(addr));
	if (rv) {
		perror("connect");
		goto out3;
	}

	backend->bot = bot;
	bot->backend = backend;
	return 0;
out3:
	fclose(backend->ws);
out2:
	close(backend->fd);
out1:
	free(backend->spill);
	free(backend);
	return -1;
}

/*
 * Handles an incoming line from signald. This could be many types of API
 * message, so we don't return an error in case we don't find the right data
 * field.
 */
static int handle_incoming(struct cbot_signal_backend *sig, struct jmsg *jm)
{
	char *msgb, *srcb, *group;

	if (jmsg_parse(jm) != 0)
		return -1;

	msgb = jmsg_lookup_string(jm, "data.dataMessage.body");
	if (!msgb)
		return 0;

	srcb = jmsg_lookup_string(jm, "data.source.number");
	if (!srcb) {
		free(msgb);
		return 0;
	}
	srcb = realloc_with_prefix(srcb, "number:");

	group = jmsg_lookup_string(jm, "data.dataMessage.groupV2.id");
	if (group)
		group = realloc_with_prefix(group, "group:");

	cbot_handle_message(sig->bot, group? group : srcb, srcb, msgb, false);
	free(group);
	free(srcb);
	free(msgb);
	return 0;
}

static void cbot_signal_run(struct cbot *bot)
{
	struct sc_lwt *cur = sc_lwt_current();
	struct cbot_signal_backend *sig = bot->backend;
	struct jmsg *jm;

	sc_lwt_wait_fd(cur, sig->fd, SC_LWT_W_IN, NULL);

	sig_expect(sig, "version");

	sig_subscribe(sig);
	sig_expect(sig, "subscribed");
	sig_expect(sig, "listen_started");

	sig_set_name(sig, bot->name);
	sig_expect(sig, "set_profile");

	while (1) {
		jm = sig_read_jmsg(sig);
		if (!jm)
			break;
		printf("\"%s\"\n", jm->orig);
		handle_incoming(sig, jm);
		jmsg_free(jm);
		jm = NULL;
	}
	exit(EXIT_FAILURE);
}

const static char fmt_send_group[] = (
	"\n{"
	    "\"username\":\"%s\","
	    "\"recipientGroupId\":\"%s\","
	    "\"messageBody\":\"%s\","
	    "\"type\":\"send\","
	    "\"version\":\"v1\""
	"}\n"
);

const static char fmt_send_single[] = (
	"\n{"
	    "\"username\":\"%s\","
	    "\"recipientAddress\":{"
	        "\"number\":\"%s\""
	    "},"
	    "\"messageBody\":\"%s\","
	    "\"type\":\"send\","
	    "\"version\":\"v1\""
	"}\n"
);

static void cbot_signal_send(const struct cbot *bot, const char *to, const char *msg)
{
	struct cbot_signal_backend *sig = bot->backend;
	const char *dest_payload;
	char *quoted = json_quote(msg);
	printf("SEND: %s\n", msg);
	if ((dest_payload = startswith(to, "phone:")))
		fprintf(sig->ws, fmt_send_single, sig->sender, dest_payload, quoted);
	else if ((dest_payload = startswith(to, "group:")))
		fprintf(sig->ws, fmt_send_group, sig->sender, dest_payload, quoted);
	else
		fprintf(stderr, "error: invalid signal destination \"%s\"\n", to);
	free(quoted);
}

struct cbot_backend_ops signal_ops = {
	.name = "signal",
	.configure = cbot_signal_configure,
	.run = cbot_signal_run,
	.send = cbot_signal_send,
};
