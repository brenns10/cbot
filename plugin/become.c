/**
 * become.c: CBot plugin which lets you tell the bot to change nick
 *
 * Sample use:
 *
 * U> cbot become newbot
 * newbot> your wish is my command
 */
#include <libconfig.h>
#include <sc-regex.h>
#include <stdlib.h>

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

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(cb, "this plugin only works on IRC or CLI!\n");
	sc_cb_concat(cb, "- cbot become <nick>: change nick\n");
}

struct cbot_plugin_ops ops = {
	.description = "cbot plugin which lets you tell the bot to change nick",
	.load = load,
	.help = help,
};
