/*
 * CBot backend implementations for Signal
 */
#include <libconfig.h>
#include <sc-collections.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../cbot_private.h"
#include "cbot/cbot.h"
#include "internal.h"
#include "nosj.h"
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
	sc_list_init(&backend->msgq);
	sc_arr_init(&backend->pending, struct signal_reaction_cb, 16);

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
	/* TODO: free all callbacks */
	sc_arr_destroy(&backend->pending);
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

static int handle_reaction(struct cbot_signal_backend *sig, struct jmsg *jm,
                           uint32_t reaction_index)
{
	uint32_t idx;
	uint64_t target_ts;
	char *emoji;
	char *srcb;
	bool remove;
	struct signal_reaction_cb cb;
	int ret =
	        jmsg_lookup_at(jm, reaction_index, "targetSentTimestamp", &idx);
	if (ret != JSON_OK) {
		CL_CRIT("reaction missing targetSentTimestamp\n");
		return -1;
	}
	ret = json_easy_number_getuint(&jm->easy, idx, &target_ts);
	if (ret != JSON_OK) {
		CL_CRIT("reaction targetSentTimestamp malformed\n");
		return -1;
	}
	if (!sig_reaction_cb(sig, target_ts, &cb)) {
		return 0;
	}
	ret = jmsg_lookup_at(jm, reaction_index, "remove", &idx);
	if (ret != JSON_OK) {
		CL_CRIT("reaction has no remove attribute\n");
		return -1;
	}
	remove = jm->easy.tokens[idx].type == JSON_TRUE;
	emoji = jmsg_lookup_string_at(jm, reaction_index, "emoji");
	if (!emoji)
		return -1;
	srcb = jmsg_lookup_string(jm, "data.source.uuid");
	if (!srcb) {
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

static bool check_waiting(struct cbot_signal_backend *sig, struct jmsg *jm)
{
	struct signal_queued_item *item;
	sc_list_for_each_entry(item, &sig->msgq, list,
	                       struct signal_queued_item)
	{
		uint32_t index;
		bool match;
		if (jmsg_lookup(jm, item->field, &index) != JSON_OK)
			continue;
		if (json_easy_string_match(&jm->easy, index, item->value,
		                           &match) != JSON_OK)
			continue;
		if (match) {
			sc_list_remove(&item->list);
			item->result = jm;
			sc_lwt_set_state(item->thread, SC_LWT_RUNNABLE);
			return true;
		}
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
	uint32_t mention_index = 0, reaction_index;

	// Uncomment below for understanding of the API requests
	// json_easy_format(&jm->easy, 0, stdout);

	if (jmsg_lookup(jm, "data.data_message.reaction", &reaction_index) ==
	    JSON_OK)
		handle_reaction(sig, jm, reaction_index);

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
		if (check_waiting(sig, jm))
			/* Handing off ownership of jm to another
			 * thread, DO NOT free it. */
			continue;
		handle_incoming(sig, jm);
		jmsg_free(jm);
		jm = NULL;
	}
	CL_CRIT("cbot signal: jmsg_read() returned NULL, exiting\n");
}

static int reaction_cmp(const struct signal_reaction_cb *lhs,
                        const struct signal_reaction_cb *rhs)
{
	if (lhs->ts < rhs->ts)
		return -1;
	else if (lhs->ts > rhs->ts)
		return 1;
	else
		return 0;
}

static void add_reaction_cb(struct cbot_signal_backend *sig, uint64_t ts,
                            const struct cbot_reaction_ops *ops, void *arg)
{
	struct signal_reaction_cb cb = { ts, *ops, arg };
	struct sc_array *a = &sig->pending;
	struct signal_reaction_cb *arr = sc_arr(a, struct signal_reaction_cb);
	size_t i;

	/* Linear search for insertion, since this is a less common case, and
	 * bsearch() does not return the correct insertion point. */

	for (i = 0; i < a->len; i++)
		if (ts < arr[i].ts)
			break;
	sc_arr_insert(a, struct signal_reaction_cb, i, cb);
}

bool sig_reaction_cb(struct cbot_signal_backend *sig, uint64_t ts,
                     struct signal_reaction_cb *out)
{
	struct signal_reaction_cb cb = { ts, { 0 }, 0 };
	struct sc_array *a = &sig->pending;
	struct signal_reaction_cb *arr = sc_arr(a, struct signal_reaction_cb);
	struct signal_reaction_cb *res =
	        bsearch(&cb, arr, a->len, sizeof(*res), (void *)reaction_cmp);
	if (res) {
		*out = *res;
		return true;
	} else {
		return false;
	}
}

static void unregister_reaction(const struct cbot *bot, uint64_t ts)
{
	struct signal_reaction_cb cb = { ts, { 0 }, 0 };
	struct cbot_signal_backend *sig = bot->backend;
	struct sc_array *a = &sig->pending;
	struct signal_reaction_cb *arr = sc_arr(a, struct signal_reaction_cb);
	struct signal_reaction_cb *res =
	        bsearch(&cb, arr, a->len, sizeof(*res), (void *)reaction_cmp);
	if (res) {
		size_t index = res - arr;
		sc_arr_remove(a, struct signal_reaction_cb, index);
	}
}

static uint64_t cbot_signal_send(const struct cbot *bot, const char *to,
                                 const struct cbot_reaction_ops *ops, void *arg,
                                 const char *msg)
{
	struct cbot_signal_backend *sig = bot->backend;
	char *dest_payload;
	int kind;
	uint64_t timestamp;

	dest_payload = mention_parse(to, &kind, NULL);
	switch (kind) {
	case MENTION_USER:
		timestamp = sig_send_single(sig, dest_payload, msg);
		break;
	case MENTION_GROUP:
		timestamp = sig_send_group(sig, dest_payload, msg);
		break;
	default:
		CL_CRIT("error: invalid signal destination \"%s\"\n", to);
		return 0;
	}
	free(dest_payload);
	if (ops && timestamp) {
		add_reaction_cb(sig, timestamp, ops, arg);
		return timestamp;
	} else {
		return 0;
	}
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

struct cbot_backend_ops signald_ops = {
	.name = "signal",
	.configure = cbot_signal_configure,
	.run = cbot_signal_run,
	.send = cbot_signal_send,
	.nick = cbot_signal_nick,
	.is_authorized = cbot_signal_is_authorized,
	.unregister_reaction = unregister_reaction,
};
