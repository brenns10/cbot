/**
 * name.c: CBot plugin which responds to questions about what CBot is, by
 * linking to the CBot repository
 */
#include "sc-regex.h"

#include "cbot/cbot.h"

struct sc_regex *r;

static void name(struct cbot_event event)
{
	// Make sure it maches our regex.
	if (sc_regex_exec(r, event.message, NULL) == -1)
		return;

	// Send our response.
	cbot_send(event.bot, event.channel,
	             "My name is CBot.  My source lives at "
	             "https://github.com/brenns10/cbot");
}

void name_load(struct cbot *bot)
{
	r = sc_regex_compile("([wW]ho|[wW]hat|[wW][tT][fF])('?s?| +"
	                     "[iI]s| +[aA]re +[yY]ou,?) +[cC][bB]ot\\??");
	cbot_register(bot, CBOT_CHANNEL_MSG, name);
}
