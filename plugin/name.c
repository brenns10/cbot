/**
 * name.c: CBot plugin which responds to questions about what CBot is, by
 * linking to the CBot repository
 */
#include "sc-regex.h"

#include "cbot/cbot.h"

struct sc_regex *r;

static void name(cbot_event_t event, cbot_actions_t actions)
{
	// Make sure it maches our regex.
	if (sc_regex_exec(r, event.message, NULL) == -1)
		return;

	// Send our response.
	actions.send(event.bot, event.channel,
	             "My name is CBot.  My source lives at "
	             "https://github.com/brenns10/cbot");
}

void name_load(cbot_t *bot, cbot_register_t registrar)
{
	r = sc_regex_compile("([wW]ho|[wW]hat|[wW][tT][fF])('?s?| +"
	                     "[iI]s| +[aA]re +[yY]ou,?) +[cC][bB]ot\\??");
	registrar(bot, CBOT_CHANNEL_MSG, name);
}
