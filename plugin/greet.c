/**
 * greet.c: CBot plugin which replies to hello messages
 *
 * Sample usage:
 *
 *     user> hi cbot
 *     cbot> hello, user!
 */
#include <stdlib.h>

#include <libconfig.h>
#include <sc-collections.h>

#include "cbot/cbot.h"

struct cbot_hello_priv {
	struct cbot_plugin *plugin;
	struct cbot_handler *hello_hdlr;
};

static void cbot_hello(struct cbot_message_event *event, void *user)
{
	cbot_send(event->bot, event->channel, "hello, %s!", event->username);
}

static void register_hello(struct cbot_hello_priv *priv)
{
	struct sc_charbuf buf;
	sc_cb_init(&buf, 256);
	sc_cb_printf(&buf, "[Hh](ello|i|ey),? +%s!?",
	             cbot_get_name(priv->plugin->bot));
	priv->hello_hdlr =
	        cbot_register(priv->plugin, CBOT_MESSAGE,
	                      (cbot_handler_t)cbot_hello, NULL, buf.buf);
	sc_cb_destroy(&buf);
}

static void cbot_bot_name_change(struct cbot_nick_event *event, void *user)
{
	struct cbot_hello_priv *priv = event->plugin->data;
	cbot_deregister(event->bot, priv->hello_hdlr);
	register_hello(priv);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	struct cbot_hello_priv *priv =
	        calloc(1, sizeof(struct cbot_hello_priv));
	plugin->data = priv;
	priv->plugin = plugin;
	register_hello(priv);
	cbot_register(plugin, CBOT_BOT_NAME,
	              (cbot_handler_t)cbot_bot_name_change, NULL, NULL);
	return 0;
}

static void unload(struct cbot_plugin *plugin)
{
	free(plugin->data);
}

struct cbot_plugin_ops ops = {
	.description = "greets people who say hello to the bot",
	.load = load,
	.unload = unload,
};
