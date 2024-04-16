/*
 * signal/backend.c: defines the common backend code for Signal. Delegates a lot
 * of functionality to a signal API bridge, but some code is shared.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libconfig.h>
#include <sc-collections.h>

#include "../cbot_private.h"
#include "cbot/cbot.h"
#include "internal.h"

static int cbot_signal_configure(struct cbot *bot, config_setting_t *group)
{
	struct cbot_signal_backend *backend;
	int rv;
	const char *phone;
	const char *uuid;
	const char *auth = NULL;
	const char *bridge;
	int ignore_dm = 0;

	rv = config_setting_lookup_string(group, "phone", &phone);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("cbot signal: key \"phone\" wrong type or not "
		        "exists\n");
		return -1;
	}

	rv = config_setting_lookup_string(group, "uuid", &uuid);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("cbot signal: key \"uuid\" wrong type or not "
		        "exists\n");
		return -1;
	}

	config_setting_lookup_string(group, "auth", &auth);

	config_setting_lookup_string(group, "bridge", &bridge);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("cbot signal: key \"bridge\" wrong type or "
		        "not found\n");
		return -1;
	}

	config_setting_lookup_bool(group, "ignore_dm", &ignore_dm);
	if (ignore_dm) {
		CL_INFO("signal: ignoring DMs\n");
	}

	backend = calloc(1, sizeof(*backend));
	backend->sender = strdup(phone);
	backend->uuid = strdup(uuid);
	backend->ignore_dm = ignore_dm;
	sc_list_init(&backend->messages);
	sc_list_init(&backend->msgq);
	sc_arr_init(&backend->pending, struct signal_reaction_cb, 16);
	if (auth)
		backend->auth_uuid = strdup(auth);

	backend->bot = bot;
	bot->backend = backend;

	if (strcmp(bridge, "signald") == 0) {
		backend->bridge = &signald_bridge;
	} else if (strcmp(bridge, "signal-cli") == 0) {
		backend->bridge = &signalcli_bridge;
	} else {
		CL_CRIT("cbot signal: unknown bridge \"%s\"\n", bridge);
		goto out;
	}

	/* Setup the real @mention (with UUID) as an alias for the bot. */
	char *alias = mention_format_p(uuid, "uuid");
	cbot_add_alias(bot, mention_format_p(uuid, "uuid"));
	free(alias);
	/* Setup the text "@cbot" and "@@cbot" as aliases for the bot. */
	asprintf(&alias, "@@%s", bot->name);
	cbot_add_alias(bot, alias);
	cbot_add_alias(bot, &alias[1]);
	free(alias);

	rv = backend->bridge->configure(bot, group);
	if (rv == 0)
		return 0;
out:
	free(backend->sender);
	free(backend->uuid);
	free(backend->auth_uuid);
	/* TODO: free all callbacks */
	sc_arr_destroy(&backend->pending);
	free(backend);
	return -1;
}

bool signal_is_group_listening(struct cbot_signal_backend *sig, const char *grp)
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
	CL_DEBUG("signal: registered reaction callback for %lu\n", ts);
	sc_arr_insert(a, struct signal_reaction_cb, i, cb);
}

bool signal_get_reaction_cb(struct cbot_signal_backend *sig, uint64_t ts,
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
	struct signal_mention *mentions;
	size_t num_mentions;

	dest_payload = mention_parse(to, &kind, NULL);
	char *quoted = json_quote_and_mention(msg, &mentions, &num_mentions);

	switch (kind) {
	case MENTION_USER:
		timestamp = sig->bridge->send_single(sig, dest_payload, quoted,
		                                     mentions, num_mentions);
		break;
	case MENTION_GROUP:
		timestamp = sig->bridge->send_group(sig, dest_payload, quoted,
		                                    mentions, num_mentions);
		break;
	default:
		CL_CRIT("error: invalid signal destination \"%s\"\n", to);
		timestamp = 0;
	}
	free(dest_payload);
	free(quoted);
	for (size_t i = 0; i < num_mentions; i++)
		free(mentions[i].uuid);
	free(mentions);
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
	sig->bridge->nick(bot, newnick);
	cbot_set_nick((struct cbot *)bot, newnick);
}

static int cbot_signal_is_authorized(const struct cbot *bot, const char *sender,
                                     const char *message)
{
	struct cbot_signal_backend *sig = bot->backend;
	int kind, rv = 0;
	char *uuid;

	if (!sig->auth_uuid)
		return 0;

	uuid = mention_parse(sender, &kind, NULL);
	if (kind != MENTION_USER) {
		free(uuid);
		return 0;
	}

	if (strcmp(uuid, sig->auth_uuid) == 0)
		rv = 1;
	free(uuid);
	return rv;
}

static void cbot_signal_run(struct cbot *bot)
{
	struct cbot_signal_backend *sig = bot->backend;
	sig->bridge->run(bot);
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
