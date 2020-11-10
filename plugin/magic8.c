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

#include "cbot/cbot.h"

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

static void magic8(struct cbot_message_event *event, void *user)
{
	int response = rand() % (sizeof(responses) / sizeof(char *));
	cbot_send(event->bot, event->channel, responses[response],
	          event->username);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)magic8, NULL,
	              "(magic8|8ball).+");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
