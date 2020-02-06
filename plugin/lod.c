/**
 * lod.c: CBot plugin which gives a look of disapproval
 *
 * Sample usage:
 *
 *     user> cbot lod libstephen
 *     cbot> ಠ_ಠ libstephen
 */

#include <stdlib.h>

#include <sc-regex.h>

#include "cbot/cbot.h"

struct sc_regex *r;

static void lod(struct cbot_event event)
{
	char *target;
	size_t *rawcap = NULL;
	int incr = cbot_addressed(event.bot, event.message);

	if (!incr)
		return;

	if (sc_regex_exec(r, event.message + incr, &rawcap) == -1) {
		free(rawcap);
		return;
	}

	target = sc_regex_get_capture(event.message + incr, rawcap, 0);
	cbot_send(event.bot, event.channel, "%s: ಠ_ಠ", target);
	free(rawcap);
	free(target);
}

void lod_load(struct cbot *bot)
{
	r = sc_regex_compile("lod\\s+(.+)\\s*");
	cbot_register(bot, CBOT_CHANNEL_MSG, lod);
}
