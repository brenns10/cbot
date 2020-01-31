/**
 * name.c: CBot plugin which responds to questions about what CBot is, by
 * linking to the CBot repository
 */

#include "cbot/cbot.h"
#include "libstephen/re.h"

Regex r;

static void name(cbot_event_t event, cbot_actions_t actions)
{
	// Make sure it maches our regex.
	if (reexec(r, event.message, NULL) == -1)
		return;

	// Send our response.
	actions.send(event.bot, event.channel,
	             "My name is CBot.  My source lives at "
	             "https://github.com/brenns10/cbot");
}

void name_load(cbot_t *bot, cbot_register_t registrar)
{
	r = recomp("([wW]ho|[wW]hat|[wW][tT][fF])('?s?| +"
	           "[iI]s| +[aA]re +[yY]ou,?) +[cC][bB]ot\\??");
	registrar(bot, CBOT_CHANNEL_MSG, name);
}
