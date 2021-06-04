#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <wait.h>
#include <assert.h>

#include <nosj.h>

#include "sc-collections.h"
#include "cbot_private.h"

const char MENTION_PLACEHOLDER[] = {0xEF, 0xBF, 0xBC, 0x00};

#define MENTION_ERR   0
#define MENTION_USER  1
#define MENTION_GROUP 2

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

static inline size_t jmsg_lookup_at(struct jmsg *jm, size_t n, char *key)
{
	return json_lookup(jm->orig, jm->tok, n, key);
}

static inline size_t jmsg_lookup(struct jmsg *jm, char *key)
{
	return jmsg_lookup_at(jm, 0, key);
}


static char *jmsg_lookup_stringnul(struct jmsg *jm, char *key, char val)
{
	size_t res = jmsg_lookup(jm, key);
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
 * Adds a prefix to the beginning of a string. Frees string and replaces it
 */
static char *format_mention(char *string, const char *prefix)
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

/*
 * Parse a mention text.
 */
static char *get_mention(const char *string, int *kind, int *offset)
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
 * Return a newly allocated string with mentions "replaced"
 *
 * Signald gives us messages with mentions in a strange format. The mentions
 * come in a JSON array, and their "start" field doesn't seem accurate. However,
 * each mention is replaced with MENTION_PLACEHOLDER, so we simply iterate over
 * each placeholder, grab a mention from the JSON list, and insert our
 * placeholder:
 *
 *   @(uuid:blah)
 *
 * Our placeholder can be translated back at the end (see below). To preserve
 * mentions which may contain @, we also identify the @ sign and double it.
 */
static char *insert_mentions(const char *str, struct jmsg *jm, size_t list)
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

/*
 * Return a newly allocated string with necessary escaping for JSON. Return a
 * second string (in mentions) which contains all mention JSON elements.
 *
 * This is called with a message text just before sending it.
 *
 * Beyond obvious JSON escaping, this function detects any mention placeholder
 * mention text:
 *   @(uuid:UUUID)
 * That text is removed and a JSON array element is created in "mentions" to
 * represent it.
 *
 * Duplicated "@@" are resolved back to "@" - this is to reverse the escaping
 * done by insert_mentions() above.
 */
static char *json_quote_and_mention(const char *instr, char **mentions)
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
			mention = get_mention(instr + i, &kind, &offset);
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
	char *msgb, *srcb, *group, *repl;
	size_t mention_index;

	if (jmsg_parse(jm) != 0)
		return -1;

	msgb = jmsg_lookup_string(jm, "data.dataMessage.body");
	if (!msgb)
		return 0;

	mention_index = jmsg_lookup(jm, "data.dataMessage.mentions");
	repl = insert_mentions(msgb, jm, mention_index);
	free(msgb);
	msgb = repl;

	srcb = jmsg_lookup_string(jm, "data.source.uuid");
	if (!srcb) {
		free(msgb);
		return 0;
	}
	srcb = format_mention(srcb, "uuid");

	group = jmsg_lookup_string(jm, "data.dataMessage.groupV2.id");
	if (group)
		group = format_mention(group, "group");

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
	    "\"mentions\":[%s],"
	    "\"type\":\"send\","
	    "\"version\":\"v1\""
	"}\n"
);

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

static void cbot_signal_send(const struct cbot *bot, const char *to, const char *msg)
{
	struct cbot_signal_backend *sig = bot->backend;
	char *dest_payload;
	char *quoted, *mentions;
	int kind;

	quoted = json_quote_and_mention(msg, &mentions);
	printf("SEND: %s\n", msg);

	dest_payload = get_mention(to, &kind, NULL);
	switch (kind) {
		case MENTION_USER:
			fprintf(sig->ws, fmt_send_single, sig->sender, dest_payload, quoted, mentions);
			break;
		case MENTION_GROUP:
			fprintf(sig->ws, fmt_send_group, sig->sender, dest_payload, quoted, mentions);
			break;
		default:
			fprintf(stderr, "error: invalid signal destination \"%s\"\n", to);
	}
	free(dest_payload);
	free(quoted);
	free(mentions);
}

struct cbot_backend_ops signal_ops = {
	.name = "signal",
	.configure = cbot_signal_configure,
	.run = cbot_signal_run,
	.send = cbot_signal_send,
};
