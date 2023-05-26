/*
 * CBot backend implementations for Signal
 */
#include <libconfig.h>
#include <sc-collections.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../cbot_private.h"
#include "cbot/cbot.h"
#include "internal.h"
#include "sc-lwt.h"

static int cbot_signal_configure(struct cbot *bot, config_setting_t *group)
{
	struct cbot_signal_backend *backend;
	struct sockaddr_un addr;
	int rv;
	const char *phone;
	const char *auth = NULL;
	const char *signald_socket;
	int ignore_dm = 0;

	rv = config_setting_lookup_string(group, "phone", &phone);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("cbot signal: key \"phone\" wrong type or not "
		        "exists\n");
		return -1;
	}

	config_setting_lookup_string(group, "auth", &auth);

	rv = config_setting_lookup_string(group, "signald_socket",
	                                  &signald_socket);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("cbot signal: key \"signald_socket\" wrong "
		        "type or not exists\n");
		return -1;
	}

	config_setting_lookup_bool(group, "ignore_dm", &ignore_dm);
	if (ignore_dm) {
		CL_INFO("signal: ignoring DMs\n");
	}

	if (strlen(signald_socket) >= sizeof(addr.sun_path)) {
		CL_CRIT("cbot signal: signald socket path too long\n");
		return -1;
	}

	backend = calloc(1, sizeof(*backend));
	backend->sender = strdup(phone);
	backend->ignore_dm = ignore_dm;
	sc_list_init(&backend->messages);

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
	if (auth)
		backend->auth = strdup(auth);

	backend->bot = bot;
	bot->backend = backend;
	return 0;
out3:
	fclose(backend->ws);
out2:
	close(backend->fd);
out1:
	sig_user_free(backend->bot_profile);
	free(backend);
	return -1;
}

static bool should_continue_group(struct cbot_signal_backend *sig,
                                  const char *grp)
{
	struct cbot *bot = sig->bot;
	struct cbot_channel_conf *chan;

	sc_list_for_each_entry(chan, &bot->init_channels, list,
	                       struct cbot_channel_conf)
	{
		if (strcmp(chan->name, grp) == 0)
			return true;
	}
	return false;
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
	uint32_t mention_index = 0;

	json_easy_format(&jm->easy, 0, stdout);

	msgb = jmsg_lookup_string(jm, "data.data_message.body");
	if (!msgb)
		return 0;

	int ret = jmsg_lookup(jm, "data.data_message.mentions", &mention_index);
	if (ret == JSON_OK) {
		repl = mention_from_json(msgb, jm, mention_index);
		free(msgb);
		msgb = repl;
	}

	srcb = jmsg_lookup_string(jm, "data.source.uuid");
	if (!srcb)
		goto out;
	srcb = mention_format(srcb, "uuid");

	group = jmsg_lookup_string(jm, "data.data_message.groupV2.id");
	if (group) {
		if (!should_continue_group(sig, group))
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

static void cbot_init_user_grp(struct cbot_signal_backend *sig)
{
	struct sc_list_head head;
	struct signal_group *grp;
	struct signal_user *user;
	int i;

	sc_list_init(&head);
	sig_list_groups(sig, &head);

	sc_list_for_each_entry(grp, &head, list, struct signal_group)
	{
		CL_INFO("Group \"%s\"\n", grp->title);
		CL_INFO("  Invite: %s\n", grp->invite_link);
		CL_INFO("  Id:     %s\n", grp->id);
		CL_INFO("  Members:\n");
		for (i = 0; i < grp->n_members; i++)
			CL_INFO("    Id: %s%s\n", grp->members[i].uuid,
			        (grp->members[i].role ==
			         SIGNAL_ROLE_ADMINISTRATOR)
			                ? " (admin)"
			                : "");
	}
	sig_group_free_all(&head);

	sc_list_init(&head);
	sig_list_contacts(sig, &head);
	sc_list_for_each_entry(user, &head, list, struct signal_user)
	{
		CL_INFO("User \"%s %s\"\n", user->first_name, user->last_name);
		CL_INFO("  Id: %s\n", user->uuid);
		CL_INFO("  Number: %s\n", user->number);
	}
	sig_user_free_all(&head);

	/* set our bot UUID mention as an alias */
	user = sig_get_profile(sig, sig->sender);
	if (user) {
		cbot_add_alias(sig->bot, mention_format_p(user->uuid, "uuid"));
		sig->bot_profile = user;
	}

	struct sc_charbuf alias;
	sc_cb_init(&alias, 64);
	sc_cb_printf(&alias, "@@%s", sig->bot->name);
	sc_cb_trim(&alias);
	cbot_add_alias(sig->bot, alias.buf);
	sc_cb_destroy(&alias);

	sig->auth_profile = sig_get_profile(sig, sig->auth);
}

static void cbot_signal_run(struct cbot *bot)
{
	struct sc_lwt *cur = sc_lwt_current();
	struct cbot_signal_backend *sig = bot->backend;
	struct jmsg *jm;

	sc_lwt_wait_fd(cur, sig->fd, SC_LWT_W_IN, NULL);

	sig_expect(sig, "version");

	cbot_init_user_grp(sig);

	if (sig_subscribe(sig) < 0)
		return;
	sig_expect(sig, "ListenerState");

	if (sig_set_name(sig, bot->name) < 0)
		return;

	while (1) {
		jm = jmsg_next(sig);
		if (!jm)
			break;
		handle_incoming(sig, jm);
		jmsg_free(jm);
		jm = NULL;
	}
	CL_CRIT("cbot signal: jmsg_read() returned NULL, exiting\n");
}

static void cbot_signal_send(const struct cbot *bot, const char *to,
                             const char *msg)
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
		CL_CRIT("error: invalid signal destination \"%s\"\n", to);
	}
	free(dest_payload);
}

static void cbot_signal_nick(const struct cbot *bot, const char *newnick)
{
	struct cbot_signal_backend *sig = bot->backend;
	sig_set_name(sig, newnick);
	cbot_set_nick((struct cbot *)bot, newnick);
}

static int cbot_signal_is_authorized(const struct cbot *bot, const char *sender,
                                     const char *message)
{
	struct cbot_signal_backend *sig = bot->backend;
	int kind, rv = 0;
	char *uuid;

	if (!sig->auth_profile)
		return 0;

	uuid = mention_parse(sender, &kind, NULL);
	if (kind != MENTION_USER) {
		free(uuid);
		return 0;
	}

	if (strcmp(uuid, sig->auth_profile->uuid) == 0)
		rv = 1;
	free(uuid);
	return rv;
}

struct cbot_backend_ops signal_ops = {
	.name = "signal",
	.configure = cbot_signal_configure,
	.run = cbot_signal_run,
	.send = cbot_signal_send,
	.nick = cbot_signal_nick,
	.is_authorized = cbot_signal_is_authorized,
};
