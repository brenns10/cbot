/**
 * tok.c: plugin for testing tokenizing
 *
 * Sample usage:
 *
 *     user> cbot tok libstephen "hello ""world""" yay
 *     cbot> [0]: libstephen
 *     cbot> [1]: hello "world"
 *     cbot> [2]: yay
 */

#include <libconfig.h>
#include <stdlib.h>

#include "cbot/cbot.h"

static void tok(struct cbot_message_event *event, void *user)
{
	struct cbot_tok tok;
	int rv = cbot_tokenize(event->message + 3, &tok);
	if (rv < 0) {
		cbot_send(event->bot, event->channel, "error: %d", rv);
		return;
	}
	for (int i = 0; i < tok.ntok; i++)
		cbot_send(event->bot, event->channel, "[%d]: %s", i,
		          tok.tokens[i]);
	cbot_tok_destroy(&tok);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)tok, NULL,
	              "tok .*");
	return 0;
}

struct cbot_plugin_ops ops = {
	.description = "a diagnostic plugin for testing tokenizers",
	.load = load,
};
