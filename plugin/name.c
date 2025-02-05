/**
 * name.c: CBot plugin which responds to questions about what CBot is, by
 * linking to the CBot repository
 */
#include <stdlib.h>

#include <libconfig.h>
#include <sc-collections.h>

#include "cbot/cbot.h"

struct cbot_karma_priv {
	struct cbot_plugin *plugin;
	struct cbot_handler *name_hdlr;
};

static void name(struct cbot_message_event *event, void *user)
{
	const char *botname = cbot_get_name(event->bot);
	cbot_send(event->bot, event->channel,
	          "My name is %s, I am a cbot.  My source lives at "
	          "https://github.com/brenns10/cbot",
	          botname);
}

static void register_name(struct cbot_karma_priv *priv)
{
	const char *botname = cbot_get_name(priv->plugin->bot);
	struct sc_charbuf buf;
	sc_cb_init(&buf, 256);
	sc_cb_printf(&buf,
	             "([wW]ho|[wW]hat|[wW][tT][fF])('?s?| +"
	             "[iI]s| +[aA]re +[yY]ou,?) +(%s|cbot)\\??",
	             botname);
	priv->name_hdlr = cbot_register(priv->plugin, CBOT_MESSAGE,
	                                (cbot_handler_t)name, NULL, buf.buf);
	sc_cb_destroy(&buf);
}

static void cbot_bot_name_change(struct cbot_nick_event *event, void *user)
{
	struct cbot_karma_priv *priv = event->plugin->data;
	cbot_deregister(event->bot, priv->name_hdlr);
	register_name(priv);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	struct cbot_karma_priv *priv =
	        calloc(1, sizeof(struct cbot_karma_priv));
	priv->plugin = plugin;
	plugin->data = priv;
	register_name(priv);
	cbot_register(plugin, CBOT_BOT_NAME,
	              (cbot_handler_t)cbot_bot_name_change, NULL, NULL);
	return 0;
}

static void unload(struct cbot_plugin *plugin)
{
	free(plugin->data);
}

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(cb, "- who/what/wtf is/are [you, ] cbot?\n");
	sc_cb_concat(cb, "  replies with bot name and github link\n");
}

struct cbot_plugin_ops ops = {
	.description = "gives information about the bot",
	.load = load,
	.unload = unload,
	.help = help,
};
