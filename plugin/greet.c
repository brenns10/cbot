/**
 * greet.c: CBot plugin which replies to hello messages
 *
 * Sample usage:
 *
 *     user> hi cbot
 *     cbot> hello, user!
 */

#include <stdlib.h>
#include <string.h>

#include "cbot/cbot.h"

static void cbot_hello(struct cbot_message_event *event, void *user)
{
	cbot_send(event->bot, event->channel, "hello, %s!", event->username);
}

void greet_load(struct cbot *bot)
{
	const char *refmt = "[Hh](ello|i|ey),? +%s!?";
	const char *name = cbot_get_name(bot);
	char *regexp = NULL;
	int len;

	len = 1 + snprintf(NULL, 0, refmt, name);
	regexp = malloc(len);
	snprintf(regexp, len, refmt, name);
	cbot_register(bot, CBOT_MESSAGE, (cbot_handler_t)cbot_hello, NULL,
	              regexp);
	free(regexp);
}
