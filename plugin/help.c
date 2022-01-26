/**
 * help.c: CBot plugin which sends help text over direct messages
 */
#include <libconfig.h>
#include <stdlib.h>

#include "cbot/cbot.h"

struct sc_regex *r;

static char *help_lines[] = {
#include "help.h"
};

static void help(struct cbot_message_event *event, void *user)
{
	size_t i;
	for (i = 0; i < sizeof(help_lines) / sizeof(char *); i++) {
		cbot_send(event->bot, event->username, help_lines[i]);
	}
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)help, NULL,
	              "[Hh][Ee][Ll][Pp].*");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
