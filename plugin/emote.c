/**
 * emote.c: CBot plugin which lets a user ask cbot to perform the "me" action.
 *
 * Sample use:
 *
 * U> cbot emote is sad
 * C> /me is sad
 * (above the /me is just used to demonstrate what CBot is doing)
 */

#include <libconfig.h>
#include <stdlib.h>

#include "cbot/cbot.h"
#include "sc-regex.h"

static void emote(struct cbot_message_event *event, void *user)
{
	char *c = sc_regex_get_capture(event->message, event->indices, 0);
	cbot_me(event->bot, event->channel, c);
	free(c);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)emote, NULL,
	              "emote (.*)");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
