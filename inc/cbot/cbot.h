/**
 * cbot.h: CBot public interface
 */

#ifndef CBOT_H
#define CBOT_H

#include <stddef.h>

struct cbot;

/**
   An enumeration of possible events that can be handled by a plugin.
 */
enum cbot_event_type {

	/* A message in a channel. */
	CBOT_CHANNEL_MSG = 0,
	/* CBot hears an action message. */
	CBOT_CHANNEL_ACTION,

	/* A user joins a channel (no regex matching here). */
	CBOT_JOIN,
	/* A user leaves a channel. */
	CBOT_PART,

	_CBOT_NUM_EVENT_TYPES_

};

/**
   Details associated with an event, which are passed to an event handler.
 */
struct cbot_event {

	struct cbot *bot;
	const char *name;

	enum cbot_event_type type;
	const char *channel;
	const char *username;
	const char *message;
};

/**
   A struct filled with actions that can be taken on a bot.
 */
struct cbot_actions {

	/**
	   Send a message to a destination.
	   @param bot The bot provided in the event struct.
	   @param dest Either a channel name or a user name.
	   @param format Format string for your message.
	   @param ... Arguments to the format string.
	 */
	void (*send)(const struct cbot *bot, const char *dest,
	             const char *format, ...);
	/**
	   Send a "me" (action) message to a destination.
	   @param bot The bot provided in the event struct.
	   @param dest Either a channel name or a user name.
	   @param format Format string for your message. No need to include the
	   bot's name or "/me".
	   @param ... Arguments to the format string.
	 */
	void (*me)(const struct cbot *bot, const char *dest, const char *format,
	           ...);
	/**
	   Return whether or not a message is addressed to the bot. When the
	   message is not addressed to the bot, returns 0. Otherwise, returns
	   the first index of the "rest" of the message.
	   @param message Message (or arbitrary string).
	   @param bot The bot the message might be addressed to.
	   @returns 0 when not addressed, otherwise index of rest of string.
	 */
	int (*addressed)(const struct cbot *bot, const char *message);
	/**
	   Determines whether the beginning of the message contains a hash that
	   matches the bot's current hash in the chain. If not, returns 0. If
	   so, returns the index of the beginning of the rest of the message.
	   @param bot The bot to check the authorization of.
	   @param message The message to check for the SHA in.
	   @returns The index of the beginning of the rest of the message.
	*/
	int (*is_authorized)(struct cbot *bot, const char *message);
	/**
	   Give operator privileges to a user.
	   @param bot Bot instance.
	   @param channel Channel to give op of.
	   @param nick Nickname of person to make op.
	 */
	void (*op)(const struct cbot *bot, const char *channel,
	           const char *message);
	/**
	   Join a channel.
	   @param bot Bot instance.
	   @param channel Channel to join.
	   @param password Password for channel, or NULL if there's none.
	 */
	void (*join)(const struct cbot *bot, const char *channel,
	             const char *password);
};

/**
   An event handler function. Takes an event and does some action to handle it.

   This is the meat of the plugin ecosystem. The main job of plugins is to write
   handlers. These handlers take event parameters, a structure filled with
   actions, and then they do something.

   @param event Structure containing details of the event to handle.
   @param actions Structure containing action functions available.
 */
typedef void (*cbot_handler_t)(struct cbot_event event,
                               struct cbot_actions actions);

/**
   An event registrar function.

   This function is passed to plugins on load, so that they may register all
   their event handlers.

   @param bot The bot you are registering with.
   @param event Event you are registering to handle.
   @param handler Event handler function.
 */
typedef void (*cbot_register_t)(struct cbot *bot, enum cbot_event_type event,
                                cbot_handler_t handler);

/**
   Main plugin loader function.

   All plugins must have a plugin loader. If they are named "plugin.c", then
   they should compile to "plugin.so", and their loader should be named
   "plugin_load", and it should have this exact signature. This function
   receives a bot instance and a registrar function. With that, it registers all
   its event handlers accordingly.

   @param bot CBot instance for the plugin.
   @param registrar Function to call to register each plugin.
 */
typedef void (*cbot_plugin_t)(struct cbot *bot, cbot_register_t registrar);

void *base64_decode(const char *str, int explen);

#endif // CBOT_H
