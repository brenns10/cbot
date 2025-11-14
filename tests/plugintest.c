/**
 * plugintest.c: tools for unit testing plugins
 */

#include "plugintest.h"
#include "../src/cbot_private.h"
#include "cbot/cbot.h"
#include "sc-collections.h"
#include "sc-lwt.h"
#include <libconfig.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Private data for the test backend
 */
struct PT_backend {
	struct sc_list_head messages;
};

static int PTB_configure(struct cbot *cbot, config_setting_t *group)
{
	struct PT_backend *backend = calloc(1, sizeof(*backend));
	sc_list_init(&backend->messages);
	cbot->backend = backend;
	return 0;
}

static void PTB_run(struct cbot *cbot)
{
	// Test backend doesn't run an event loop
	// Tests directly inject events
}

static uint64_t PTB_send(const struct cbot *cbot, const char *to,
                         const struct cbot_reaction_ops *ops, void *arg,
                         const char *msg)
{
	struct PT_backend *backend = cbot->backend;
	struct PT_message *tm = calloc(1, sizeof(*tm));

	tm->dest = strdup(to);
	tm->msg = strdup(msg);
	tm->is_me = false;
	sc_list_insert_end(&backend->messages, &tm->list);

	return 0; // No reaction support in test backend
}

static void PTB_me(const struct cbot *cbot, const char *to, const char *msg)
{
	struct PT_backend *backend = cbot->backend;
	struct PT_message *tm = calloc(1, sizeof(*tm));

	tm->dest = strdup(to);
	tm->msg = strdup(msg);
	tm->is_me = true;
	sc_list_insert_end(&backend->messages, &tm->list);
}

static void PTB_op(const struct cbot *cbot, const char *channel,
                   const char *username)
{
	// No-op for testing
}

static void PTB_join(const struct cbot *cbot, const char *channel,
                     const char *password)
{
	// No-op for testing
}

static void PTB_nick(const struct cbot *cbot, const char *newnick)
{
	// No-op for testing
}

static int PTB_is_authorized(const struct cbot *bot, const char *sender,
                             const char *message)
{
	// For testing, you could make this configurable
	return 0;
}

static void PTB_unregister_reaction(const struct cbot *bot, uint64_t id)
{
	// No-op for testing
}

struct cbot_backend_ops test_ops = {
	.name = "test",
	.configure = PTB_configure,
	.run = PTB_run,
	.send = PTB_send,
	.me = PTB_me,
	.op = PTB_op,
	.join = PTB_join,
	.nick = PTB_nick,
	.is_authorized = PTB_is_authorized,
	.unregister_reaction = PTB_unregister_reaction,
};

struct cbot *PT_bot_create(const char *name)
{
	struct cbot *bot = cbot_create();

	// cbot_create() already initializes all the lists and aliases
	bot->name = strdup(name);
	bot->backend_name = strdup("test");
	bot->backend_ops = &test_ops;

	// Initialize the backend
	PTB_configure(bot, NULL);

	// Initialize database (in-memory)
	bot->db_file = strdup(":memory:");
	if (cbot_db_init(bot) != 0) {
		cbot_delete(bot);
		return NULL;
	}

	// Initialize LWT context (needed for plugins that use timers/async)
	bot->lwt_ctx = sc_lwt_init();

	return bot;
}

static void PT_message_free(struct PT_message *msg)
{
	if (!msg)
		return;
	free(msg->dest);
	free(msg->msg);
	free(msg);
}

static void PT_messages_free_all(struct sc_list_head *head)
{
	struct PT_message *msg, *tmp;
	sc_list_for_each_safe(msg, tmp, head, list, struct PT_message)
	{
		sc_list_remove(&msg->list);
		PT_message_free(msg);
	}
}

void PT_bot_destroy(struct cbot *bot)
{
	if (!bot)
		return;

	struct PT_backend *backend = bot->backend;
	if (backend) {
		PT_messages_free_all(&backend->messages);
		free(backend);
		bot->backend = NULL;
	}

	// cbot_delete will free lwt_ctx and other resources
	cbot_delete(bot);
}

void PT_inject_message(struct cbot *bot, const char *channel, const char *user,
                       const char *message, bool is_action, bool is_dm)
{
	cbot_handle_message(bot, channel, user, message, is_action, is_dm);
}

void PT_messages_clear(struct cbot *bot)
{
	struct PT_backend *backend = bot->backend;
	PT_messages_free_all(&backend->messages);
	sc_list_init(&backend->messages);
}

int PT_messages_count(struct cbot *bot)
{
	struct PT_backend *backend = bot->backend;
	int count = 0;
	struct PT_message *msg;
	sc_list_for_each_entry(msg, &backend->messages, list, struct PT_message)
	{
		count++;
	}
	return count;
}

struct PT_message *PT_messages_get(struct cbot *bot, int n)
{
	struct PT_backend *backend = bot->backend;
	int i = 0;
	struct PT_message *msg;
	sc_list_for_each_entry(msg, &backend->messages, list, struct PT_message)
	{
		if (i == n)
			return msg;
		i++;
	}
	return NULL;
}

struct cbot_plugin *
PT_load_plugin(struct cbot *bot, struct cbot_plugin_ops *ops, const char *name)
{
	struct cbot_plugpriv *priv = calloc(1, sizeof(*priv));
	if (!priv)
		return NULL;

	priv->name = strdup(name);
	priv->bot = bot;
	priv->p.ops = ops;
	priv->p.bot = bot;
	priv->p.data = NULL;

	sc_list_init(&priv->handlers);
	sc_list_insert_end(&bot->plugins, &priv->list);

	// Call the plugin's load function
	if (ops->load && ops->load(&priv->p, NULL) != 0) {
		sc_list_remove(&priv->list);
		free(priv->name);
		free(priv);
		return NULL;
	}

	return &priv->p;
}

void PT_unload_plugin(struct cbot_plugin *plugin)
{
	if (!plugin)
		return;

	struct cbot_plugpriv *priv = plugpriv(plugin);

	// Call the plugin's unload function if it exists
	if (plugin->ops->unload)
		plugin->ops->unload(plugin);

	// Deregister all handlers
	struct cbot_handler *hdlr, *tmp;
	sc_list_for_each_safe(hdlr, tmp, &priv->handlers, plugin_list,
	                      struct cbot_handler)
	{
		cbot_deregister(priv->bot, hdlr);
	}

	// Remove from plugin list
	sc_list_remove(&priv->list);

	free(priv->name);
	free(priv);
}
