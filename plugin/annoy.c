/**
 * annoy.c: CBot plugin which sends annoying messages to a channel
 *
 * Sample usage:
 *
 *     user> cbot be annoying
 *     cbot> hello! im an annoying bot
 *     cbot> hello! im an annoying bot
 *     cbot> hello! im an annoying bot
 *     cbot> hello! im an annoying bot
 *     ...
 *     user> cbot stop it
 */

#include <libconfig.h>
#include <sc-lwt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cbot/cbot.h"

char *channel = NULL;
struct cbot *bot = NULL;

static void annoy_loop(void *data)
{
	struct timespec ts;
	ts.tv_sec = 3;
	ts.tv_nsec = 0;
	while (channel) {
		printf("sending annoing message to %s\n", channel);
		cbot_send(bot, channel, "hello! im an annoying bot");
		sc_lwt_sleep(&ts);
		if (sc_lwt_shutting_down())
			return;
	}
}

static void start(struct cbot_message_event *event, void *user)
{
	printf("being annoying to %s\n", event->channel);
	channel = strdup(event->channel);
	bot = event->bot;
	sc_lwt_create_task(cbot_get_lwt_ctx(event->bot), annoy_loop, NULL);
}

static void stop(struct cbot_message_event *event, void *user)
{
	free(channel);
	channel = NULL;
	bot = NULL;
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)start, NULL,
	              "be annoying");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)stop, NULL,
	              "stop it!?");
	return 0;
}

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(
	        cb,
	        "- cbot be annoying: send annoying messages to a channel\n");
	sc_cb_concat(cb, "- cbot stop it: stop sending annoying messages\n");
}

struct cbot_plugin_ops ops = {
	.description = "a plugin that sends annoying messages every 3 seconds",
	.load = load,
	.help = help,
};
