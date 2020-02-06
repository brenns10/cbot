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

static void help(cbot_event_t event, cbot_actions_t actions)
{
	// Make sure the message is addressed to the bot.
	int increment = actions.addressed(event.bot, event.message);
	if (!increment)
		return;

	// Make sure the message matches our regex.
	if (sc_regex_exec(r, event.message + increment, NULL) == -1)
		return;

	// Send the help text.
	size_t i;
	for (i = 0; i < sizeof(help_lines) / sizeof(char *); i++) {
		actions.send(event.bot, event.username, help_lines[i]);
	}
}

void help_load(cbot_t *bot, cbot_register_t registrar)
{
	r = sc_regex_compile("[Hh][Ee][Ll][Pp].*");
	registrar(bot, CBOT_CHANNEL_MSG, help);
}
