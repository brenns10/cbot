/**
 * reactrack.c: CBot plugin which tests out the ability to receive reactions and
 * act upon them
 */
#include <libconfig.h>
#include <stdlib.h>
#include <string.h>

#include <cbot/cbot.h>

static int react(struct cbot_reaction_event *event, void *arg)
{
	if (event->remove) {
		cbot_send(event->plugin->bot, arg,
		          "%s removed \"%s\" from my message!\n", event->source,
		          event->emoji);
	} else if (strcmp(event->emoji, "🛑") == 0) {
		cbot_send(event->plugin->bot, arg,
		          "%s has told me to stop watching for reacts",
		          event->source);
		cbot_unregister_reaction(event->bot, event->handle);
		free(arg);
	} else {
		cbot_send(event->plugin->bot, arg,
		          "%s reacted \"%s\" to my message!\n", event->source,
		          event->emoji);
	}
	return 0;
}

static struct cbot_reaction_ops react_ops = {
	.plugin = NULL,
	.react_fn = react,
};

static void reply(struct cbot_message_event *event, void *user)
{
	void *arg = strdup(event->channel);
	cbot_sendr(event->bot, event->channel, &react_ops, arg,
	           "React to this message");
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	react_ops.plugin = plugin;

	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)reply, NULL,
	              "react");
	return 0;
}

struct cbot_plugin_ops ops = {
	.description = "notices when you react to its message",
	.load = load,
};
