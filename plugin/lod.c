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

static void lod(struct cbot_message_event *event, void *user)
{
	char *target;
	target = sc_regex_get_capture(event->message, event->indices, 0);
	cbot_send(event->bot, event->channel, "%s: ಠ_ಠ", target);
	free(target);
}

void lod_load(struct cbot *bot)
{
	cbot_register(bot, CBOT_ADDRESSED, (cbot_handler_t)lod, NULL,
	              "lod\\s+(.+)\\s*");
}
