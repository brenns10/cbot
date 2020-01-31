/**
 * emote.c: CBot plugin which lets a user ask cbot to perform the "me" action.
 *
 * Sample use:
 *
 * U> cbot emote is sad
 * C> /me is sad
 * (above the /me is just used to demonstrate what CBot is doing)
 */

#include "cbot/cbot.h"
#include "libstephen/re.h"

Regex r;

static void emote(cbot_event_t event, cbot_actions_t actions)
{
	size_t *captures = NULL;
	int incr = actions.addressed(event.bot, event.message);

	if (!incr)
		return;

	event.message += incr;

	if (reexec(r, event.message, &captures) == -1) {
		return;
	}

	Captures c = recap(event.message, captures, renumsaves(r));

	actions.me(event.bot, event.channel, c.cap[0]);

	recapfree(c);
}

void emote_load(cbot_t *bot, cbot_register_t registrar)
{
	r = recomp("emote (.*)");
	registrar(bot, CBOT_CHANNEL_MSG, emote);
}
