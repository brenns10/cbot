/**
 * greet.c: CBot plugin which replies to hello messages
 *
 * Sample usage:
 *
 *     user> hi cbot
 *     cbot> hello, user!
 */

#include <string.h>

#include "cbot/cbot.h"

static void cbot_hello(struct cbot_message_event *event, void *user)
{
	cbot_send(event->bot, event->channel, "hello, %s!", event->username);
}

void greet_load(struct cbot *bot)
{
	cbot_register(bot, CBOT_MESSAGE, (cbot_handler_t)cbot_hello, NULL,
	              "[Hh](ello|i|ey),? +[Cc][Bb]ot!?");
}
