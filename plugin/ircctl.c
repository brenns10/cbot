/**
 * ircctl.c: CBot plugin which allows users to request IRC operations like
 * giving op privilege, or joining a channel
 */
#include <stdlib.h>

#include "sc-regex.h"

#include "cbot/cbot.h"

#define HASH "([A-Za-z0-9+=/]+)"

static void op_handler(struct cbot_message_event *event, void *user_data)
{
	char *hash, *user;
	user = sc_regex_get_capture(event->message, event->indices, 0);
	hash = sc_regex_get_capture(event->message, event->indices, 1);
	if (cbot_is_authorized(event->bot, hash))
		cbot_op(event->bot, event->channel, user);
	else
		cbot_send(event->bot, event->channel,
		          "Sorry, you aren't authorized to do that.");
	free(user);
	free(hash);
}

static void join_handler(struct cbot_message_event *event, void *user)
{
	char *hash, *channel;
	channel = sc_regex_get_capture(event->message, event->indices, 0);
	hash = sc_regex_get_capture(event->message, event->indices, 1);
	if (cbot_is_authorized(event->bot, hash))
		cbot_join(event->bot, channel, NULL);
	else
		cbot_send(event->bot, event->channel,
		          "Sorry, you aren't authorized to do that.");
	free(channel);
	free(hash);
}

void ircctl_load(struct cbot *bot)
{
	cbot_register(bot, CBOT_ADDRESSED, (cbot_handler_t)op_handler, NULL,
	              "op +(.*) +" HASH);
	cbot_register(bot, CBOT_ADDRESSED, (cbot_handler_t)join_handler, NULL,
	              "join +(.*) +" HASH);
}
