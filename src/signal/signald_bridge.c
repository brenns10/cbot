/*
 * signal/signald_bridge.c: code that communicates with signald bridge
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <libconfig.h>
#include <nosj.h>
#include <sc-collections.h>
#include <sc-lwt.h>

#include "../cbot_private.h"
#include "cbot/cbot.h"
#include "internal.h"

static struct jmsg *signald_get_result(struct cbot_signal_backend *sig,
                                       const char *type)
{
	char buf[16];
	struct jmsg *jm;
	CL_DEBUG("sig_result: wait for id %lu, type \"%s\"\n", sig->id - 1,
	         type);

	snprintf(buf, sizeof(buf), "%lu", sig->id - 1);
	jm = jmsg_wait_field(sig, "id", buf);

	if (!je_string_match(&jm->easy, 0, "type", type)) {
		char *actual = NULL;
		je_get_string(&jm->easy, 0, "type", &actual);
		CL_CRIT("error: response to request %lu does was \"%s\", not "
		        "\"%s\"\n",
		        sig->id - 1, actual ? actual : "(unknown)", type);
		free(actual);
		jmsg_free(jm);
		return NULL;
	}

	CL_DEBUG("sig_result: wait for id %lu completed\n", sig->id - 1);
	return jm;
}

static int signald_result(struct cbot_signal_backend *sig, const char *type)
{
	struct jmsg *jm = signald_get_result(sig, type);
	if (jm) {
		jmsg_free(jm);
		return 0;
	} else {
		return -1;
	}
}

static void signald_expect(struct cbot_signal_backend *sig, const char *type)
{
	CL_DEBUG("sig_expect: \"%s\"\n", type);
	struct jmsg *jm = jmsg_wait_field(sig, "type", type);
	jmsg_free(jm);
	CL_DEBUG("sig_expect: completed \"%s\"\n", type);
}

static uint64_t get_timestamp(struct jmsg *jm)
{
	uint64_t timestamp;
	int ret = je_get_uint(&jm->easy, 0, "data.timestamp", &timestamp);
	if (ret != JSON_OK) {
		CL_CRIT("failed to get data.timestamp field in message: %s\n",
		        json_strerror(ret));
		return 0;
	}
	return timestamp;
}

static int signald_subscribe(struct cbot_signal_backend *sig)
{
	char fmt[] = "\n{\"id\":\"%lu\",\"version\":\"v1\","
	             "\"type\":\"subscribe\",\"account\":\"%s\"}\n";
	fprintf(sig->ws, fmt, sig->id++, sig->sender);
	return signald_result(sig, "subscribe");
}

static void signald_nick(const struct cbot *bot, const char *newnick)
{
	struct cbot_signal_backend *sig = bot->backend;
	fprintf(sig->ws,
	        "\n{\"id\":\"%lu\",\"account\":\"%s\",\"name\":\"%s\",\"type\":"
	        "\"set_profile\",\"version\":\"v1\"}\n",
	        sig->id++, sig->sender, newnick);
	signald_result(sig, "set_profile");
}

static char *format_mentions(const struct signal_mention *ms, size_t n)
{
	struct sc_charbuf cb;
	sc_cb_init(&cb, 128);

	for (size_t i = 0; i < n; i++) {
		if (i)
			sc_cb_append(&cb, ',');
		sc_cb_printf(&cb,
		             "{\"length\": %" PRIu64 ", \"start\": %" PRIu64
		             ", \"uuid\": \"%s\"}",
		             ms[i].length, ms[i].start, ms[i].uuid);
	}
	return cb.buf;
}

const static char fmt_send_group[] = ("\n{"
                                      "\"id\": \"%lu\","
                                      "\"username\":\"%s\","
                                      "\"recipientGroupId\":\"%s\","
                                      "\"messageBody\":\"%s\","
                                      "\"mentions\":[%s],"
                                      "\"type\":\"send\","
                                      "\"version\":\"v1\""
                                      "}\n");

static uint64_t signald_send_group(struct cbot_signal_backend *sig,
                                   const char *to, const char *quoted,
                                   const struct signal_mention *ms, size_t n)
{
	struct jmsg *jm;
	uint64_t timestamp = 0;
	char *mentions = format_mentions(ms, n);
	fprintf(sig->ws, fmt_send_group, sig->id++, sig->sender, to, quoted,
	        mentions);
	free(mentions);
	jm = signald_get_result(sig, "send");
	/* jm could be NULL when shutting down */
	if (jm) {
		timestamp = get_timestamp(jm);
		jmsg_free(jm);
	}
	return timestamp;
}

const static char fmt_send_single[] = ("\n{"
                                       "\"id\": \"%lu\","
                                       "\"username\":\"%s\","
                                       "\"recipientAddress\":{"
                                       "\"uuid\":\"%s\""
                                       "},"
                                       "\"messageBody\":\"%s\","
                                       "\"mentions\":[%s],"
                                       "\"type\":\"send\","
                                       "\"version\":\"v1\""
                                       "}\n");

static uint64_t signald_send_single(struct cbot_signal_backend *sig,
                                    const char *to, const char *quoted,
                                    const struct signal_mention *ms, size_t n)
{
	struct jmsg *jm;
	uint64_t timestamp;
	char *mentions = format_mentions(ms, n);
	fprintf(sig->ws, fmt_send_single, sig->id++, sig->sender, to, quoted,
	        mentions);
	free(mentions);
	jm = signald_get_result(sig, "send");
	timestamp = get_timestamp(jm);
	jmsg_free(jm);
	return timestamp;
}

static int handle_reaction(struct cbot_signal_backend *sig, struct jmsg *jm,
                           uint32_t reaction_index)
{
	uint64_t target_ts;
	char *emoji;
	char *srcb;
	bool remove;
	struct signal_reaction_cb cb;
	int ret = je_get_uint(&jm->easy, reaction_index, "targetSentTimestamp",
	                      &target_ts);
	if (ret != JSON_OK) {
		CL_CRIT("error accessing targetSentTimestamp in reaction\n");
		return -1;
	}
	if (!signal_get_reaction_cb(sig, target_ts, &cb)) {
		return 0;
	}
	ret = je_get_bool(&jm->easy, reaction_index, "remove", &remove);
	if (ret != JSON_OK) {
		CL_CRIT("reaction has no remove attribute\n");
		return -1;
	}
	ret = je_get_string(&jm->easy, reaction_index, "emoji", &emoji);
	if (ret != JSON_OK)
		return -1;
	ret = je_get_string(&jm->easy, 0, "data.source.uuid", &srcb);
	if (ret != JSON_OK) {
		free(emoji);
		return -1;
	}
	srcb = mention_format(srcb, "uuid");

	CL_DEBUG("Sending reaction \"%s\" %s to message ts %lu to plugin\n",
	         emoji, remove ? "remove" : "add", target_ts);
	struct cbot_reaction_event evt = {
		.plugin = cb.ops.plugin,
		.bot = sig->bot,
		.emoji = emoji,
		.source = srcb,
		.remove = remove,
		.handle = target_ts,
	};
	cb.ops.react_fn(&evt, cb.arg);
	free(emoji);
	free(srcb);
	return 0;
}

/*
 * Handles an incoming line from signald. This could be many types of API
 * message, so we don't return an error in case we don't find the right data
 * field.
 */
static int handle_incoming(struct cbot_signal_backend *sig, struct jmsg *jm)
{
	char *msgb = NULL;
	char *srcb = NULL;
	char *group = NULL;
	char *repl;
	uint32_t mention_index = 0, reaction_index;

	// Uncomment below for understanding of the API requests
	// json_easy_format(&jm->easy, 0, stdout);

	int ret = je_get_object(&jm->easy, 0, "data.data_message.reaction",
	                        &reaction_index);
	if (ret == JSON_OK)
		handle_reaction(sig, jm, reaction_index);

	ret = je_get_string(&jm->easy, 0, "data.data_message.body", &msgb);
	if (ret != JSON_OK)
		return 0;

	ret = je_get_array(&jm->easy, 0, "data.data_message.mentions",
	                   &mention_index);
	if (ret == JSON_OK) {
		repl = mention_from_json(msgb, &jm->easy, mention_index);
		free(msgb);
		msgb = repl;
	}

	ret = je_get_string(&jm->easy, 0, "data.source.uuid", &srcb);
	if (ret != JSON_OK)
		goto out;
	srcb = mention_format(srcb, "uuid");

	je_get_string(&jm->easy, 0, "data.data_message.groupV2.id", &group);
	if (ret == JSON_OK) {
		if (!signal_is_group_listening(sig, group))
			goto out;
		group = mention_format(group, "group");
	} else if (sig->ignore_dm) {
		goto out;
	}

	if (group)
		cbot_handle_message(sig->bot, group, srcb, msgb, false, false);
	else
		cbot_handle_message(sig->bot, srcb, srcb, msgb, false, true);
out:
	free(group);
	free(srcb);
	free(msgb);
	return 0;
}

static void signald_run(struct cbot *bot)
{
	struct sc_lwt *cur = sc_lwt_current();
	struct cbot_signal_backend *sig = bot->backend;
	struct jmsg *jm;

	sc_lwt_wait_fd(cur, sig->fd, SC_LWT_W_IN, NULL);

	signald_expect(sig, "version");

	if (signald_subscribe(sig) < 0)
		return;
	signald_expect(sig, "ListenerState");
	signald_nick(bot, bot->name);

	while (1) {
		jm = jmsg_next(sig);
		if (!jm)
			break;
		if (jmsg_deliver(sig, jm))
			/* Handing off ownership of jm to another
			 * thread, DO NOT free it. */
			continue;
		handle_incoming(sig, jm);
		jmsg_free(jm);
		jm = NULL;
	}
	CL_CRIT("cbot signal: jmsg_read() returned NULL, exiting\n");
}

static int signald_configure(struct cbot *bot, config_setting_t *group)
{
	struct cbot_signal_backend *sig = bot->backend;
	const char *signald_socket;
	struct sockaddr_un addr;

	int rv = config_setting_lookup_string(group, "signald_socket",
	                                      &signald_socket);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("cbot signal: key \"signald_socket\" required for "
		        "signald bridge\n");
		return -1;
	}
	if (strlen(signald_socket) >= sizeof(addr.sun_path)) {
		CL_CRIT("cbot signal: signald socket path too long\n");
		return -1;
	}

	sig->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sig->fd < 0) {
		perror("create socket");
		return -1;
	}
	sig->ws = fdopen(sig->fd, "w");
	if (!sig->ws) {
		perror("fdopen socket");
		close(sig->fd);
		return -1;
	}
	setvbuf(sig->ws, NULL, _IONBF, 0);

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, signald_socket, sizeof(addr.sun_path));
	rv = connect(sig->fd, (struct sockaddr *)&addr, sizeof(addr));
	if (rv) {
		perror("connect");
		fclose(sig->ws);
		close(sig->fd);
		return -1;
	}
	return 0;
}

struct signal_bridge_ops signald_bridge = {
	.send_single = signald_send_single,
	.send_group = signald_send_group,
	.nick = signald_nick,
	.run = signald_run,
	.configure = signald_configure,
};
