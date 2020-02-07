/**
 * help.c: CBot plugin which sends help text over direct messages
 */
#include "sc-regex.h"

#include <stdlib.h>

#include "cbot/cbot.h"

struct sc_regex *r;

static char *help_lines[] = {
#include "help.h"
};

static void help(struct cbot_message_event *event, void *user)
{
	size_t i;
	for (i = 0; i < sizeof(help_lines) / sizeof(char *); i++) {
		cbot_send(event->bot, event->username, help_lines[i]);
	}
}

void help_load(struct cbot *bot)
{
	cbot_register(bot, CBOT_ADDRESSED, (cbot_handler_t)help, NULL,
	              "[Hh][Ee][Ll][Pp].*");
}
