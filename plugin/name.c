/**
 * name.c: CBot plugin which responds to questions about what CBot is, by
 * linking to the CBot repository
 */
#include "cbot/cbot.h"

static void name(struct cbot_message_event *event, void *user)
{
	cbot_send(event->bot, event->channel,
	          "My name is CBot.  My source lives at "
	          "https://github.com/brenns10/cbot");
}

void name_load(struct cbot *bot)
{
	char *r = ("([wW]ho|[wW]hat|[wW][tT][fF])('?s?| +"
	           "[iI]s| +[aA]re +[yY]ou,?) +[cC][bB]ot\\??");
	cbot_register(bot, CBOT_MESSAGE, (cbot_handler_t)name, NULL, r);
}
