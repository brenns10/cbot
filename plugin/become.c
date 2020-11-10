/**
 * become.c: CBot plugin which lets you tell the bot to change nick
 *
 * Sample use:
 *
 * U> cbot become newbot
 * newbot> your wish is my command
 */
#include <stdlib.h>

#include <sc-regex.h>

#include "cbot/cbot.h"

static void become(struct cbot_message_event *event, void *user)
{
	char *name = sc_regex_get_capture(event->message, event->indices, 0);
	cbot_nick(event->bot, name);
	free(name);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)become, NULL,
	              "become (\\w+)");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
