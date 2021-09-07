/*
 * CBot backend implementations for Signal
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sc-collections.h>

#include "../cbot_private.h"
#include "internal.h"

static int cbot_signal_configure(struct cbot *bot, config_setting_t *group)
{
	struct cbot_signal_backend *backend;
	struct sockaddr_un addr;
	int rv;
	const char *phone;
	const char *signald_socket;

	rv = config_setting_lookup_string(group, "phone", &phone);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr, "cbot signal: key \"phone\" wrong type or not exists\n");
		return -1;
	}

	rv = config_setting_lookup_string(group, "signald_socket", &signald_socket);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr, "cbot signal: key \"signald_socket\" wrong type or not exists\n");
		return -1;
	}
	if (strlen(signald_socket) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "cbot signal: signald socket path too long\n");
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

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, signald_socket, sizeof(addr.sun_path));
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
	repl = mention_from_json(msgb, jm, mention_index);
	free(msgb);
	msgb = repl;

	srcb = jmsg_lookup_string(jm, "data.source.uuid");
	if (!srcb) {
		free(msgb);
		return 0;
	}
	srcb = mention_format(srcb, "uuid");

	group = jmsg_lookup_string(jm, "data.dataMessage.groupV2.id");
	if (group)
		group = mention_format(group, "group");

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

	sig_list_groups(sig);

	sig_subscribe(sig);
	sig_expect(sig, "listen_started");

	sig_set_name(sig, bot->name);

	while (1) {
		jm = jmsg_read(sig);
		if (!jm)
			break;
		printf("\"%s\"\n", jm->orig);
		handle_incoming(sig, jm);
		jmsg_free(jm);
		jm = NULL;
	}
	exit(EXIT_FAILURE);
}

static void cbot_signal_send(const struct cbot *bot, const char *to, const char *msg)
{
	struct cbot_signal_backend *sig = bot->backend;
	char *dest_payload;
	int kind;

	dest_payload = mention_parse(to, &kind, NULL);
	switch (kind) {
		case MENTION_USER:
			sig_send_single(sig, dest_payload, msg);
			break;
		case MENTION_GROUP:
			sig_send_group(sig, dest_payload, msg);
			break;
		default:
			fprintf(stderr, "error: invalid signal destination \"%s\"\n", to);
	}
	free(dest_payload);
}

static void cbot_signal_nick(const struct cbot *bot, const char *newnick)
{
	struct cbot_signal_backend *sig = bot->backend;
	sig_set_name(sig, newnick);
	cbot_set_nick((struct cbot *)bot, newnick);
}

struct cbot_backend_ops signal_ops = {
	.name = "signal",
	.configure = cbot_signal_configure,
	.run = cbot_signal_run,
	.send = cbot_signal_send,
	.nick = cbot_signal_nick,
};
