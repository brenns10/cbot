/**
 * ircctl.c: CBot plugin which allows users to request IRC operations like
 * giving op privilege, or joining a channel
 */
#include <libconfig.h>
#include <stdlib.h>

#include "cbot/cbot.h"
#include "sc-regex.h"

static void op_handler(struct cbot_message_event *event, void *user_data)
{
	char *user;
	user = sc_regex_get_capture(event->message, event->indices, 0);
	if (cbot_is_authorized(event->bot, event->username, event->message))
		cbot_op(event->bot, event->channel, user);
	else
		cbot_send(event->bot, event->channel,
		          "Sorry, you aren't authorized to do that.");
	free(user);
}

static void join_handler(struct cbot_message_event *event, void *user)
{
	char *channel;
	channel = sc_regex_get_capture(event->message, event->indices, 0);
	if (cbot_is_authorized(event->bot, event->username, event->message))
		cbot_join(event->bot, channel, NULL);
	else
		cbot_send(event->bot, event->channel,
		          "Sorry, you aren't authorized to do that.");
	free(channel);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)op_handler, NULL,
	              "op +(.*) ?.*");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)join_handler,
	              NULL, "join +(.*) ?.*");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
