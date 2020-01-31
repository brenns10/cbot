/**
 * lod.c: CBot plugin which gives a look of disapproval
 *
 * Sample usage:
 *
 *     user> cbot lod libstephen
 *     cbot> ಠ_ಠ libstephen
 */

#include <stdlib.h>

#include "cbot/cbot.h"
#include "libstephen/re.h"
#include "libstephen/cb.h"

Regex r;

static void lod(cbot_event_t event, cbot_actions_t actions)
{
	Captures captures;
	size_t *rawcap = NULL;
	int incr = actions.addressed(event.bot, event.message);

	if (!incr)
		return;

	if (reexec(r, event.message + incr, &rawcap) == -1) {
		free(rawcap);
		return;
	}

	captures = recap(event.message + incr, rawcap, renumsaves(r));
	actions.send(event.bot, event.channel, "%s: ಠ_ಠ", captures.cap[0]);
	recapfree(captures);
	free(rawcap);
}

void lod_load(cbot_t *bot, cbot_register_t registrar)
{
	r = recomp("lod\\s+(.+)\\s*");
	registrar(bot, CBOT_CHANNEL_MSG, lod);
}
