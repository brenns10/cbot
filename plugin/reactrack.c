/**
 * reactrack.c: CBot plugin which tests out the ability to receive reactions and
 * act upon them
 */
#include <string.h>

#include <cbot/cbot.h>

static int react(struct cbot_plugin *plugin, void *arg, char *emoji,
                 bool remove)
{
	if (remove)
		cbot_send(plugin->bot, arg,
		          "You removed \"%s\" from my message!\n", emoji);
	else
		cbot_send(plugin->bot, arg,
		          "You reacted \"%s\" to my message!\n", emoji);
	return 0;
}

static void reply(struct cbot_message_event *event, void *user)
{
	struct cbot_reaction_ops ops = {
		.arg = strdup(event->channel),
		.plugin = event->plugin,
		.react_fn = react,
		.free_fn = NULL,
	};
	cbot_sendr(event->bot, event->channel, &ops, "React to this message");
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)reply, NULL,
	              "react");
	return 0;
}

struct cbot_plugin_ops ops = {
	.description = "notices when you react to its message",
	.load = load,
};
