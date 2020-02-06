/**
 * sadness.c: CBot plugin which replies to mean words with a randomly selected
 * insult from a curated list
 */

#include <stdlib.h>

#include "sc-regex.h"

#include "cbot/cbot.h"

struct sc_regex *r;
char *responses[] = {
	":(",
	"I don't like you, %s",
	"http://foaas.com/field/cbot/%s/Bible",
	"http://foaas.com/yoda/%s/cbot",
	"http://foaas.com/shutup/%s/cbot",
	"http://foaas.com/cool/cbot",
	"http://foaas.com/fascinating/cbot",
	"http://foaas.com/single/cbot",
	"http://foaas.com/pulp/C/cbot",
	"http://foaas.com/horse/cbot",
	"http://foaas.com/too/cbot",
	"http://foaas.com/outside/%s/cbot",
	"http://foaas.com/thanks/cbot",
	"http://foaas.com/madison/%s/cbot",
	"http://foaas.com/flying/cbot",
};

static void sadness(struct cbot_event event, struct cbot_actions actions)
{
	int incr = actions.addressed(event.bot, event.message);
	if (!incr)
		return;

	if (sc_regex_exec(r, event.message + incr, NULL) == -1)
		return;

	actions.send(event.bot, event.channel,
	             responses[rand() % (sizeof(responses) / sizeof(char *))],
	             event.username);
}

void sadness_load(struct cbot *bot)
{
	r = sc_regex_compile("([Yy]ou +[Ss]uck[!.]?|"
	                     "[Ss]ucks[!.]?|"
	                     "[Ii] +[Hh]ate +[Yy]ou[!.]?|"
	                     "[Ss]hut [Uu]p[!.]?)");
	cbot_register(bot, CBOT_CHANNEL_MSG, sadness);
}
