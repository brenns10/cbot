/**
 * greet.c: CBot plugin which replies to hello messages
 *
 * Sample usage:
 *
 *     user> hi cbot
 *     cbot> hello, user!
 */

#include <string.h>

#include "sc-regex.h"

#include "cbot/cbot.h"

struct sc_regex *greeting;

static void cbot_hello(struct cbot_event event, struct cbot_actions actions)
{
	if (sc_regex_exec(greeting, event.message, NULL) == -1)
		return;
	actions.send(event.bot, event.channel, "hello, %s!", event.username);
}

void greet_load(struct cbot *bot)
{
	greeting = sc_regex_compile("[Hh](ello|i|ey),? +[Cc][Bb]ot!?");
	cbot_register(bot, CBOT_CHANNEL_MSG, cbot_hello);
}
