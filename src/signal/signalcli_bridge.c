/*
 * signal/signalcli_bridge.c: code that communicates to signal-cli's jsonRpc API
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libconfig.h>
#include <nosj.h>
#include <sc-collections.h>
#include <sc-lwt.h>

#include "../cbot_private.h"
#include "cbot/cbot.h"
#include "cbot/json.h"
#include "internal.h"

static struct jmsg *get_result(struct cbot_signal_backend *sig)
{
	char buf[16];
	CL_DEBUG("sig_result: wait for result of id %lu\n", sig->id - 1);
	snprintf(buf, sizeof(buf), "%lu", sig->id - 1);
	return jmsg_wait_field(sig, "id", buf);
}

static uint64_t get_timestamp(struct jmsg *jm)
{
	uint64_t timestamp;
	int ret = je_get_uint(&jm->easy, 0, "result.timestamp", &timestamp);
	if (ret != JSON_OK) {
		CL_CRIT("failed to get timestamp field in message: %s\n",
		        json_strerror(ret));
		return 0;
	}
	return timestamp;
}

static char *format_mentions(const struct signal_mention *ms, size_t n)
{
	struct sc_charbuf cb;
	sc_cb_init(&cb, 128);

	for (size_t i = 0; i < n; i++) {
		if (i)
			sc_cb_append(&cb, ',');
		sc_cb_printf(&cb, "\"%" PRIu64 ":%" PRIu64 ":%s\"", ms[i].start,
		             ms[i].length, ms[i].uuid);
	}
	return cb.buf;
}

const static char fmt_send[] =
        ("{\"jsonrpc\":\"2.0\",\"method\":\"send\",\"id\":\"%lu\","
         "\"params\":{\"message\":\"%s\",\"%s\":\"%s\",\"mentions\":[%s]}"
         "}\n");

static uint64_t signalcli_send(struct cbot_signal_backend *sig, const char *to,
                               const char *quoted,
                               const struct signal_mention *ms, size_t n,
                               const char *key)
{

	uint64_t timestamp = 0;
	char *mentions = format_mentions(ms, n);
	fprintf(sig->ws, fmt_send, sig->id++, quoted, key, to, mentions);
	free(mentions);
	struct jmsg *jm = get_result(sig);
	/* jm could be NULL when shutting down */
	if (jm) {
		timestamp = get_timestamp(jm);
		jmsg_free(jm);
	}
	return timestamp;
}

static uint64_t signalcli_send_single(struct cbot_signal_backend *sig,
                                      const char *to, const char *quoted,
                                      const struct signal_mention *ms, size_t n)
{
	return signalcli_send(sig, to, quoted, ms, n, "recipient");
}

static uint64_t signalcli_send_group(struct cbot_signal_backend *sig,
                                     const char *to, const char *quoted,
                                     const struct signal_mention *ms, size_t n)
{
	return signalcli_send(sig, to, quoted, ms, n, "groupId");
}

static const char fmt_nick[] =
        ("\{\"jsonrpc\":\"2.0\",\"method\":\"updateProfile\","
         "\"id\":\"%lu\",\"params\":{\"name\":\"%s\","
         "\"aboutEmoji\":\"🤖\",\"about\",\"I'm a bot! "
         "https://github.com/brenns10/cbot\"}}\n");

static void signalcli_nick(const struct cbot *bot, const char *newnick)
{
	struct cbot_signal_backend *sig = bot->backend;
	char *quoted = json_quote_nomention(newnick);
	fprintf(sig->ws, fmt_nick, sig->id++, quoted);
	free(quoted);
}

static int handle_reaction(struct cbot_signal_backend *sig, struct jmsg *jm,
                           const char *srcb, uint32_t reaction_index)
{
	uint64_t target_ts;
	char *emoji;
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
	ret = je_get_bool(&jm->easy, reaction_index, "isRemove", &remove);
	if (ret != JSON_OK) {
		CL_CRIT("reaction has no remove attribute\n");
		return -1;
	}
	ret = je_get_string(&jm->easy, reaction_index, "emoji", &emoji);
	if (ret != JSON_OK)
		return -1;
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
	return 0;
}

static int handle_incoming(struct cbot_signal_backend *sig, struct jmsg *jm)
{
	int ret;
	char *msgb = NULL;
	char *srcb = NULL;
	char *group = NULL;
	char *repl;
	uint32_t mention_index = 0, reaction_index;

	// Uncomment below for understanding of the API requests
	// json_easy_format(&jm->easy, 0, stdout);

	if (!je_string_match(&jm->easy, 0, "method", "receive")) {
		CL_DEBUG("skip non-receive message\n");
		return 0;
	}

	ret = je_get_string(&jm->easy, 0, "params.envelope.sourceUuid", &srcb);
	if (ret != JSON_OK) {
		CL_DEBUG("skip message without sourceUuid\n");
		return 0;
	}
	srcb = mention_format(srcb, "uuid");
	je_get_string(&jm->easy, 0,
	              "params.envelope.dataMessage.groupInfo.groupId", &group);

	if (group && !signal_is_group_listening(sig, group)) {
		CL_DEBUG("skip message to group we don't care about\n");
		goto out;
	} else if (group) {
		group = mention_format(group, "group");
	} else if (sig->ignore_dm) {
		CL_DEBUG("skip dm\n");
		goto out;
	}

	ret = je_get_object(&jm->easy, 0,
	                    "params.envelope.dataMessage.reaction",
	                    &reaction_index);
	if (ret == JSON_OK) {
		CL_DEBUG("handle reaction\n");
		handle_reaction(sig, jm, srcb, reaction_index);
		goto out;
	}

	ret = je_get_string(&jm->easy, 0, "params.envelope.dataMessage.message",
	                    &msgb);
	if (ret != JSON_OK) {
		// CL_DEBUG("fail to load message body\n");
		goto out;
	}

	ret = je_get_array(&jm->easy, 0, "params.envelope.dataMessage.mentions",
	                   &mention_index);
	if (ret == JSON_OK) {
		CL_DEBUG("parsing mentions\n");
		repl = mention_from_json(msgb, &jm->easy, mention_index);
		free(msgb);
		msgb = repl;
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

static void signalcli_run(struct cbot *bot)
{
	struct sc_lwt *cur = sc_lwt_current();
	struct cbot_signal_backend *sig = bot->backend;
	struct jmsg *jm;

	sc_lwt_wait_fd(cur, sig->fd, SC_LWT_W_IN, NULL);

	CL_INFO("signalcli: running\n");
	signalcli_nick(bot, bot->name);

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

static int pipecmd(struct cbot_signal_backend *sig, char *cmd, int *input_fd,
                   int *output_fd)
{
	int stdin[2];
	int stdout[2];
#define READ  0
#define WRITE 1
	pipe(stdin);
	pipe(stdout);
	pid_t pid = fork();
	if (pid == -1) {
		CL_CRIT("cbot signal: fork failed: %s\n", strerror(errno));
		return -1;
	} else if (pid == 0) {
		/* child process: close the write end of stdin */
		close(stdin[WRITE]);
		/* child process: close the read end of stdout */
		close(stdout[READ]);

		/* duplicate the read end of stdin and then close pipe fd */
		dup2(stdin[READ], STDIN_FILENO);
		close(stdin[READ]);

		/* same for write end of stdout */
		dup2(stdout[WRITE], STDOUT_FILENO);
		close(stdout[WRITE]);

		char *args[] = { "sh", "-c", cmd, NULL };
		execvp("sh", args);
		perror("execvp");
		/* the only possible error handling is exiting */
		exit(1);
	} else {
		sig->child = pid;

		/* parent process: close the read end of stdin */
		close(stdin[READ]);
		/* and the write end of stdout */
		close(stdout[WRITE]);

		/* ensure stdout (which we read from) is non-blocking */
		fcntl(stdout[READ], F_SETFL,
		      fcntl(stdout[READ], F_GETFL) | O_NONBLOCK);

		*input_fd = stdin[WRITE];
		*output_fd = stdout[READ];
		return 0;
	}
#undef READ
#undef WRITE
}

static int signalcli_configure(struct cbot *bot, config_setting_t *group)
{
	struct cbot_signal_backend *sig = bot->backend;
	const char *signalcli_cmd;
	int rv = config_setting_lookup_string(group, "signalcli_cmd",
	                                      &signalcli_cmd);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("cbot signal: key \"signalcli_cmd\" required for "
		        "signal-cli bridge\n");
		return -1;
	}

	rv = pipecmd(sig, (char *)signalcli_cmd, &sig->write_fd, &sig->fd);
	if (rv < 0)
		return rv;

	sig->ws = fdopen(sig->write_fd, "w");
	if (!sig->ws) {
		perror("fdopen pipe");
		close(sig->fd);
		close(sig->write_fd);
		kill(sig->child, SIGTERM);
		waitpid(sig->child, NULL, 0);
		return -1;
	}
	setvbuf(sig->ws, NULL, _IONBF, 0);
	return 0;
}

struct signal_bridge_ops signalcli_bridge = {
	.send_single = signalcli_send_single,
	.send_group = signalcli_send_group,
	.nick = signalcli_nick,
	.run = signalcli_run,
	.configure = signalcli_configure,
};
