/**
 * help.c: CBot plugin which sends help text over direct messages
 */
#include <libconfig.h>
#include <microhttpd.h>
#include <stdlib.h>

#include "cbot/cbot.h"
#include "sc-collections.h"

#include "../src/cbot_private.h"

static void help(struct cbot_message_event *event, void *user)
{
	const char *url = cbot_http_geturl(event->bot);
	cbot_send(event->bot, event->channel,
	          "Please see CBot's user documentation at "
	          "http://brenns10.github.io/cbot/User.html");
	if (url)
		cbot_send(event->bot, event->channel,
		          "See also this bot's help pages at %s/help", url);
}

static void http_get(struct cbot_http_event *event, void *user)
{
	struct sc_charbuf cb;
	struct cbot_plugpriv *plug;

	cbot_http_plainresp_start(&cb, "CBot Help");

	sc_cb_concat(
	        &cb,
	        "CBot Plugin Help\n"
	        "================\n"
	        "\n"
	        "Welcome to the web-based help for CBot. Here you may find\n"
	        "a list of all the enabled plugins, with a bit of help about\n"
	        "them. If you want to see full details about how each plugin\n"
	        "works and other documentation info, please refer to the \n"
	        "full documentation <a "
	        "href=\"http://brenns10.github.io/cbot/\">here</a>.\n"
	        "\n");

	sc_list_for_each_entry(plug, &event->bot->plugins, list,
	                       struct cbot_plugpriv)
	{

		sc_cb_printf(&cb, "\nPlugin: %s\n", plug->name);
		if (plug->p.ops->description)
			sc_cb_printf(&cb, "  %s\n", plug->p.ops->description);
		if (plug->p.ops->help) {
			sc_cb_printf(&cb, "Help:\n");
			plug->p.ops->help(&plug->p, &cb);
		}
	}

	cbot_http_plainresp_send(&cb, event, MHD_HTTP_OK);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)help, NULL,
	              "[Hh][Ee][Ll][Pp].*");

	cbot_register(plugin, CBOT_HTTP_GET, (cbot_handler_t)http_get, NULL,
	              "/help");
	return 0;
}

struct cbot_plugin_ops ops = {
	.description = "print this help message and serve help webpage",
	.load = load,
};
