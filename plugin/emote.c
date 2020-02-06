/**
 * emote.c: CBot plugin which lets a user ask cbot to perform the "me" action.
 *
 * Sample use:
 *
 * U> cbot emote is sad
 * C> /me is sad
 * (above the /me is just used to demonstrate what CBot is doing)
 */

#include <stdlib.h>

#include "sc-regex.h"

#include "cbot/cbot.h"

struct sc_regex *r;
const int num_captures = 1;

static void emote(struct cbot_event event, struct cbot_actions actions)
{
	size_t *indices;
	int incr = actions.addressed(event.bot, event.message);

	if (!incr)
		return;

	event.message += incr;

	if (sc_regex_exec(r, event.message, &indices) == -1) {
		return;
	}

	char *c = sc_regex_get_capture(event.message, indices, 0);
	actions.me(event.bot, event.channel, c);
	free(c);
	free(indices);
}

void emote_load(struct cbot *bot)
{
	r = sc_regex_compile("emote (.*)");
	cbot_register(bot, CBOT_CHANNEL_MSG, emote);
}
