/**
 * help.c: CBot plugin which sends help text over direct messages
 */
#include <libconfig.h>
#include <stdlib.h>

#include "cbot/cbot.h"

static void help(struct cbot_message_event *event, void *user)
{
	cbot_send(event->bot, event->channel,
	          "Please see CBot's user documentation at "
	          "http://brenns10.github.io/cbot/User.html");
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
