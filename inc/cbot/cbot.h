/**
 * cbot.h: CBot public interface
 */

#ifndef CBOT_H
#define CBOT_H

#include <stddef.h>

#include "sc-collections.h"

struct cbot;

struct cbot_handler;

/**
 * An enumeration of possible events that can be handled by a plugin.
 */
enum cbot_event_type {
	/* Any action message in a channel */
	CBOT_MESSAGE,
	/* Any message addressed to CBot */
	CBOT_ADDRESSED,

	/* A user joins a channel */
	CBOT_JOIN,
	/* A user leaves a channel */
	CBOT_PART,

	/* A user changes nickname (not the bot) */
	CBOT_NICK,

	/* The bot's name changes */
	CBOT_BOT_NAME,

	_CBOT_NUM_EVENT_TYPES_

};

/**
 * A general event type. This holds only a reference to the bot, and the event
 * type. From this information, it should be possible to cast a pointer to a
 * more specific event type (such as cbot_regex_event).
 */
struct cbot_event {
	struct cbot *bot;
	enum cbot_event_type type;
};
struct cbot_message_event {
	struct cbot *bot;
	enum cbot_event_type type;
	const char *channel;
	const char *username;
	const char *message;
	bool is_action;
	size_t *indices;
	int num_captures;
};
struct cbot_user_event {
	struct cbot *bot;
	enum cbot_event_type type; /* CBOT_JOIN, CBOT_PART */
	const char *channel;
	const char *username;
};
struct cbot_nick_event {
	struct cbot *bot;
	enum cbot_event_type type; /* CBOT_NICK, CBOT_BOT_NAME */
	const char *old_username;
	const char *new_username;
};

struct cbot_user_info {
	char *username;
	char *realname;
	struct sc_list_head list;
};

/**
 * Send a message to a destination.
 * @param bot The bot provided in the event struct.
 * @param dest Either a channel name or a user name.
 * @param format Format string for your message.
 * @param ... Arguments to the format string.
 */
void cbot_send(const struct cbot *bot, const char *dest, const char *format,
               ...);
/**
 * Send a "me" (action) message to a destination.
 * @param bot The bot provided in the event struct.
 * @param dest Either a channel name or a user name.
 * @param format Format string for your message. No need to include the
 * bot's name or "/me".
 * @param ... Arguments to the format string.
 */
void cbot_me(const struct cbot *bot, const char *dest, const char *format, ...);
/**
 * Return whether or not a message is addressed to the bot. When the
 * message is not addressed to the bot, returns 0. Otherwise, returns
 * the first index of the "rest" of the message.
 * @param message Message (or arbitrary string).
 * @param bot The bot the message might be addressed to.
 * @returns 0 when not addressed, otherwise index of rest of string.
 */
int cbot_addressed(const struct cbot *bot, const char *message);
/**
 * Determines whether the beginning of the message contains a hash that
 * matches the bot's current hash in the chain. If not, returns 0. If
 * so, returns the index of the beginning of the rest of the message.
 * @param bot The bot to check the authorization of.
 * @param message The message to check for the SHA in.
 * @returns The index of the beginning of the rest of the message.
 */
int cbot_is_authorized(struct cbot *bot, const char *message);
/**
 * Give operator privileges to a user.
 * @param bot Bot instance.
 * @param channel Channel to give op of.
 * @param nick Nickname of person to make op.
 */
void cbot_op(const struct cbot *bot, const char *channel, const char *message);
/**
 * Join a channel.
 * @param bot Bot instance.
 * @param channel Channel to join.
 * @param password Password for channel, or NULL if there's none.
 */
void cbot_join(const struct cbot *bot, const char *channel,
               const char *password);
/**
 * Change the bot's nickname.
 * @param bot Bot instance
 * @param newname New name to set
 */
void cbot_nick(const struct cbot *bot, const char *newname);

/**
 * An event handler function. Takes an event and does some action to handle it.
 *
 * This is the meat of the plugin ecosystem. The main job of plugins is to write
 * handlers. These handlers take event parameters and then they do something.
 *
 * @param event Structure containing details of the event to handle.
 * @param user User data for the handler
 */
typedef void (*cbot_handler_t)(struct cbot_event *event, void *user);

/**
 * @brief Register a handler for an event
 * @param bot Handle to the cbot instance
 * @param event Event type you are registering to handle
 * @param handler Event handler callback
 * @param user User pointer for this function
 * @param regex Regular expression (if applicable)
 * @returns The "cbot_handler" pointer - an opaque type which is used to
 *   deregister a plugin.
 */
struct cbot_handler *cbot_register(struct cbot *bot, enum cbot_event_type type,
                                   cbot_handler_t handler, void *user,
                                   char *regex);

/**
 * @brief Deregister an existing handler
 * @param bot The bot instance
 * @param hdlr Handler pointer returned by cbot_register()
 */
void cbot_deregister(struct cbot *bot, struct cbot_handler *hdlr);

/**
 * Main plugin loader function signture.
 *
 * All plugins must have a plugin loader. If they are named "plugin.c", then
 * they should compile to "plugin.so", and their loader should be named
 * "plugin_load", and it should have this exact signature. This function
 * receives a bot instance and a registrar function. With that, it registers all
 * its event handlers accordingly.
 *
 * @param bot CBot instance for the plugin.
 */
typedef void (*cbot_plugin_t)(struct cbot *bot);

/**
 * Return a linked list of channel members.
 *
 * This function inserts into the list head pointed by `head`, a structure for
 * each user which is a member of a channel. The structure is of type
 * `struct cbot_user_info`, and the links are via the `list` field. The function
 * returns the number of users added to this list, or a negative number on any
 * error.
 *
 * The resulting list should be freed with sc_user_info_free_all(), or
 * individual elements should be freed with sc_user_info_free().
 *
 * @param bot CBot instance for the plugin
 * @param chan Name of the channel
 * @param head List head into which we store users
 * @returns Number of users, or a negative error code
 */
int cbot_get_members(struct cbot *bot, char *chan, struct sc_list_head *head);

/**
 * Free the user_info structure.
 *
 * String fields are owned by the structure, and are freed along with it.
 *
 * @param info User info to free
 */
void cbot_user_info_free(struct cbot_user_info *info);

/**
 * Free all user_info in the list
 * @param head List to free
 */
void cbot_user_info_free_all(struct sc_list_head *head);

/**
 * Return the bot name.
 *
 * The returned string may not be modified or freed by the caller. The name may
 * at any point be freed by an IRC action. Thus, if the name is to be stored
 * long term, it should be copied and stored elsewhere, to avoid dereferencing
 * a freed name pointer later.
 *
 * @param bot Bot to get the name of
 * @returns Bot name
 */
const char *cbot_get_name(struct cbot *bot);

/**
 * Return the bot's lightweight thread context.
 *
 * This can be used to schedule threads to run on the event loop. Plugin should
 * behave, and not hog the CPU. Otherwise the whole system becomes sad.
 *
 * @param bot the bot instance
 * @returns the lwt_ctx
 */
struct sc_lwt_ctx *cbot_get_lwt_ctx(struct cbot *bot);

/*****************
 * Tokenizing API
 *****************/

struct cbot_tok {
	char *original; // copy of the tokenized string
	char **tokens;  // pointers into original for each token
	int ntok;       // count of tokens in array
};

/**
 * Tokenize `msg` (copy it first) and fill `*result` with the tokens.
 * Return count of tokens (also stored in ntok) on success, negative on fail.
 */
int cbot_tokenize(const char *msg, struct cbot_tok *result);

/**
 * Frees resources held by `tokens`. Does not free the actual tokens struct.
 */
void cbot_tok_destroy(struct cbot_tok *tokens);

#endif // CBOT_H
