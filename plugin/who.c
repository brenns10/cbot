/**
 * who.c: CBot plugin which lists members of this channel
 *
 * Sample usage:
 *
 *     user> hi cbot
 *     cbot> hello, user!
 */

#include <libconfig.h>
#include <sc-collections.h>
#include <stddef.h>

#include "cbot/cbot.h"

static void who(struct cbot_message_event *event, void *user)
{
	struct sc_list_head list;
	struct cbot_user_info *ui;
	struct sc_charbuf buf;

	sc_list_init(&list);
	sc_cb_init(&buf, 512);

	cbot_get_members(event->bot, (char *)event->channel, &list);
	cbot_send(event->bot, event->channel,
	          "Members of channel %s (\"censoring\" to avoid ping)",
	          event->channel);
	sc_list_for_each_entry(ui, &list, list, struct cbot_user_info)
	{
		sc_cb_printf(&buf, "%c*%s ", ui->username[0], &ui->username[1]);
		if (buf.length >= 500) {
			cbot_send(event->bot, event->channel, buf.buf);
			sc_cb_clear(&buf);
		}
	}
	if (buf.length > 0) {
		cbot_send(event->bot, event->channel, buf.buf);
	}
	sc_cb_destroy(&buf);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)who, NULL,
	              "[wW]ho\\??");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
