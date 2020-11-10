/**
 * sadness.c: CBot plugin which replies to mean words with a randomly selected
 * insult from a curated list
 */

#include <stdlib.h>

#include "cbot/cbot.h"

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

static void sadness(struct cbot_message_event *event, void *user)
{
	cbot_send(event->bot, event->channel,
	          responses[rand() % (sizeof(responses) / sizeof(char *))],
	          event->username);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_MESSAGE, (cbot_handler_t)sadness, NULL,
	              "([Yy]ou +[Ss]uck[!.]?|"
	              "[Ss]ucks[!.]?|"
	              "[Ii] +[Hh]ate +[Yy]ou[!.]?|"
	              "[Ss]hut [Uu]p[!.]?)");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
