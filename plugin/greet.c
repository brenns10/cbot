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
#include "libstephen/re.h"

Regex greeting;

static void cbot_hello(cbot_event_t event, cbot_actions_t actions)
{
	if (reexec(greeting, event.message, NULL) == -1)
		return;
	actions.send(event.bot, event.channel, "hello, %s!", event.username);
}

void greet_load(cbot_t *bot, cbot_register_t registrar)
{
	greeting = recomp("[Hh](ello|i|ey),? +[Cc][Bb]ot!?");
	registrar(bot, CBOT_CHANNEL_MSG, cbot_hello);
}
