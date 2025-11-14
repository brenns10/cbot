/**
 * plugintest.h: tools for unit testing plugins
 *
 * Contains a bot backend for testing plugins, as well as helper functions for
 * testing them. All declarations here should be prefixed "PT_" to indicate
 * they're plugin test helpers, not test functions or part of the cbot API.
 */

#ifndef TEST_BACKEND_H
#define TEST_BACKEND_H

#include "cbot/cbot.h"
#include <sc-collections.h>

/**
 * A captured message from the bot
 */
struct PT_message {
	char *dest;
	char *msg;
	bool is_me; // true if this was a /me action
	struct sc_list_head list;
};

/**
 * Initialize a test bot with the test backend.
 * Returns a configured bot ready for testing.
 *
 * @param name Bot name
 * @returns Configured cbot instance
 */
struct cbot *PT_bot_create(const char *name);

/**
 * Clean up a test bot
 */
void PT_bot_destroy(struct cbot *bot);

/**
 * Inject a message event into the bot for the plugin to maybe handle
 *
 * The message doesn't get enqueued into the list of PT_messages -- only
 * responses from the plugin will be there.
 *
 * @param bot Bot instance
 * @param channel Channel name (or username for DMs)
 * @param user Username sending the message
 * @param message Message text
 * @param is_action True if this is a /me action
 * @param is_dm True if this is a direct message
 */
void PT_inject_message(struct cbot *bot, const char *channel, const char *user,
                       const char *message, bool is_action, bool is_dm);

void PT_messages_clear(struct cbot *bot);
int PT_messages_count(struct cbot *bot);
struct PT_message *PT_messages_get(struct cbot *bot, int n);

/**
 * Load a plugin by its operations structure.
 * This is for statically-linked plugins in tests.
 *
 * @param bot Bot instance
 * @param ops Plugin operations
 * @param name Plugin name
 * @returns Plugin instance, or NULL on failure
 */
struct cbot_plugin *
PT_load_plugin(struct cbot *bot, struct cbot_plugin_ops *ops, const char *name);

/**
 * Unload a plugin and free its resources
 */
void PT_unload_plugin(struct cbot_plugin *plugin);

#endif // TEST_BACKEND_H
