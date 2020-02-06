/**
 * magic8: CBot plugin which answers magic 8 ball queries
 *
 * Sample usage:
 *
 *     user> cbot magic8 will you reply?
 *     cbot> It is decidedly so
 *
 *     user> cbot 8ball should I use C++
 *     cbot> Outlook not so good
 */

#include <stdio.h>
#include <stdlib.h>

#include "sc-regex.h"

#include "cbot/cbot.h"

struct sc_regex *query;

char *responses[] = {
	"It is certain.",
	"It is decidedly so.",
	"Without a doubt.",
	"Yes - definitely.",
	"You may rely on it.",
	"As I see it, yes.",
	"Most likely.",
	"Outlook good.",
	"Yes.",
	"Signs point to yes.",
	"Reply hazy, try again.",
	"Ask again later.",
	"Better not tell you now.",
	"Cannot predict now.",
	"Concentrate and ask again.",
	"Don't count on it.",
	"My reply is no.",
	"My sources say no.",
	"Outlook not so good.",
	"Very doubtful.",
};

static void magic8(struct cbot_event event, struct cbot_actions actions)
{
	int incr = actions.addressed(event.bot, event.message);

	if (!incr)
		return;

	if (sc_regex_exec(query, event.message + incr, NULL) == -1)
		return;

	int response = rand() % (sizeof(responses) / sizeof(char *));
	actions.send(event.bot, event.channel, responses[response],
	             event.username);
}

void magic8_load(struct cbot *bot)
{
	query = sc_regex_compile("(magic8|8ball).+");
	cbot_register(bot, CBOT_CHANNEL_MSG, magic8);
}
