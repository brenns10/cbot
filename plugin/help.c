/**
 * help.c: CBot plugin which sends help text over direct messages
 */

#include <stdlib.h>

#include "cbot/cbot.h"
#include "libstephen/re.h"

Regex r;

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
	if (reexec(r, event.message + increment, NULL) == -1)
		return;

	// Send the help text.
	size_t i;
	for (i = 0; i < sizeof(help_lines) / sizeof(char *); i++) {
		actions.send(event.bot, event.username, help_lines[i]);
	}
}

void help_load(cbot_t *bot, cbot_register_t registrar)
{
	r = recomp("[Hh][Ee][Ll][Pp].*");
	registrar(bot, CBOT_CHANNEL_MSG, help);
}
