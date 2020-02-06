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

static void help(struct cbot_event event)
{
	// Make sure the message is addressed to the bot.
	int increment = cbot_addressed(event.bot, event.message);
	if (!increment)
		return;

	// Make sure the message matches our regex.
	if (sc_regex_exec(r, event.message + increment, NULL) == -1)
		return;

	// Send the help text.
	size_t i;
	for (i = 0; i < sizeof(help_lines) / sizeof(char *); i++) {
		cbot_send(event.bot, event.username, help_lines[i]);
	}
}

void help_load(struct cbot *bot)
{
	r = sc_regex_compile("[Hh][Ee][Ll][Pp].*");
	cbot_register(bot, CBOT_CHANNEL_MSG, help);
}
