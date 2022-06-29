/**
 * cbot.h: CBot public interface
 */

#ifndef CBOT_H
#define CBOT_H

#include <libconfig.h>
#include <sqlite3.h>
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

	/* HTTP Requests */
	CBOT_HTTP_ANY,
	CBOT_HTTP_GET,

	_CBOT_NUM_EVENT_TYPES_

};

/**
 * A general event type. This holds only a reference to the bot, and the event
 * type. From this information, it should be possible to cast a pointer to a
 * more specific event type (such as cbot_regex_event).
 */
struct cbot_event {
	struct cbot *bot;
	struct cbot_plugin *plugin;
	enum cbot_event_type type;
};
struct cbot_message_event {
	struct cbot *bot;
	struct cbot_plugin *plugin;
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
	struct cbot_plugin *plugin;
	enum cbot_event_type type; /* CBOT_JOIN, CBOT_PART */
	const char *channel;
	const char *username;
};
struct cbot_nick_event {
	struct cbot *bot;
	struct cbot_plugin *plugin;
	enum cbot_event_type type; /* CBOT_NICK, CBOT_BOT_NAME */
	const char *old_username;
	const char *new_username;
};
struct cbot_http_event {
	struct cbot *bot;
	struct cbot_plugin *plugin;
	enum cbot_event_type type; /* CBOT_HTTP_{ANY,GET,...} */

	/* URL match */
	size_t *indices;
	int num_captures;

	/* MHD info */
	struct MHD_Connection *connection;
	const char *url;
	const char *method;
	const char *version;
	const char *upload_data;
	size_t upload_data_size;
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
 * Send a message to a destination, rate limited.
 * Same args as cbot_send.
 */
void cbot_send_rl(struct cbot *cbot, const char *dest, const char *format, ...);
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
 * Determines whether the sender / message is authorized.
 * This just means it has privilege -- usually meaning the user is the bot
 * author, i.e. Me :D
 * @param bot The bot instance
 * @param user The user to check the authorization of
 * @param message The message in question
 * @returns non-zero if authorized
 */
int cbot_is_authorized(struct cbot *bot, const char *user, const char *message);
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
 * Add an alias for the bot.
 *
 * Bot aliases are helpful for backends which have special encodings for
 * "mentions". Backends can add their aliases here and the bot will recognize
 * this when used to address the bot.
 *
 * @param bot Bot instance
 * @param alias Name to add
 */
void cbot_add_alias(struct cbot *bot, const char *alias);

struct cbot_plugin;

struct cbot_plugin_ops {
	char *description;

	/**
	 * Load the plugin. Return 0 on success. Return negative on failure,
	 * in which case the plugin's resources should be completely freed, as
	 * it will not be added to the bot.
	 * @param plugin A plugin handle unique to this plugin instance
	 * @param config Configuration
	 * @retval integer status
	 */
	int (*load)(struct cbot_plugin *plugin, config_setting_t *config);

	/**
	 * (optional) Unload the plugin. This is mainly called on shutdown, but
	 * could also be called by user request to unload particular plugins.
	 * Note that the plugin is *NOT* responsible for undoing its handler
	 * registrations via cbot_deregister(). This callback is merely for
	 * *additional* cleanup which may not otherwise be done.
	 *
	 * If this field is NULL, then the plugin will not receive any
	 * notification before being unloaded.
	 *
	 * @param plugin Plugin instance
	 */
	void (*unload)(struct cbot_plugin *plugin);

	/**
	 * (optional) Format a help message
	 */
	void (*help)(struct cbot_plugin *plugin, struct sc_charbuf *cb);
};

/*
 * The public plugin interface. A pointer this struct serves as an identifying
 * handle for plugins, as it is passed to plugin functions, and used by plugins
 * to identify themselves when calling core functions.
 */
struct cbot_plugin {
	struct cbot_plugin_ops *ops;
	void *data;
	struct cbot *bot;
};

/**
 * Unload a plugin. Note that the plugin itself should not directly call this,
 * as the plugin code would be unloaded by it. As an alternative, a plugin may
 * do the following:
 *
 *     sc_lwt_create_task(cbot_get_lwt_ctx(), cbot_unload_plugin, plugin);
 *
 * This would schedule an asynchronous task to unload the plugin.
 */
void cbot_unload_plugin(struct cbot_plugin *plugin);

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
 *
 * This function may soon be deprecated in favor of cbot_register2.
 *
 * @param plugin The plugin of this event
 * @param event Event type you are registering to handle
 * @param handler Event handler callback
 * @param user User pointer for this function
 * @param regex Regular expression (if applicable)
 * @returns The "cbot_handler" pointer - an opaque type which is used to
 *   deregister a plugin.
 */
struct cbot_handler *cbot_register(struct cbot_plugin *plugin,
                                   enum cbot_event_type type,
                                   cbot_handler_t handler, void *user,
                                   char *regex);

/**
 * @brief Register a handler for an event
 *
 * This function may soon be deprecated in favor of cbot_register2.
 *
 * @param plugin The plugin of this event
 * @param event Event type you are registering to handle
 * @param handler Event handler callback
 * @param user User pointer for this function
 * @param regex Regular expression (if applicable)
 * @param re_flags Flags passed to sc_regex_compile2()
 * @returns The "cbot_handler" pointer - an opaque type which is used to
 *   deregister a plugin.
 */
struct cbot_handler *cbot_register2(struct cbot_plugin *plugin,
                                    enum cbot_event_type type,
                                    cbot_handler_t handler, void *user,
                                    char *regex, int re_flags);

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

/******************
 * Dynamic Formatting API
 ******************/

typedef int (*cbot_formatter_t)(struct sc_charbuf *, char *, void *);

int cbot_format(struct sc_charbuf *buf, const char *fmt,
                cbot_formatter_t formatter, void *user);

/******************
 * DB API
 ******************/

/**
 * This struct is necessary to register a table with cbot. It enables you to
 * update your table schema in a backward-compatible way. As you make changes to
 * your schema, you increment its version, and you add an alter statement to the
 * alters array. On startup, the bot checks for the existence of the table, and
 * its current schema version. If the version is below your specified version,
 * it will run each alter statement in sequence, bringing your table up-to-date.
 *
 * If your table change is backward compatible, you must set the alters array
 * member to NULL. The migration will continue until the first NULL alter
 * statement, at which point it will fail. You must include an entry in your
 * changelog documentation which instructs the user on how to migrate past this
 * point.
 */
struct cbot_db_table {
	const char *name;
	const char *create;
	const char **alters;
	unsigned int version;
};

/**
 * @brief Register a database table. This should be called during plugin init.
 *
 * See documentation for struct cbot_db_table for the details on the
 * registration process and how the struct should be filled in.
 *
 * @param plugin The plugin registering this table.
 * @param tbl The table object. This should point to memory which is valid for
 *    the entire lifetime of the bot, ideally static memory.
 */
int cbot_db_register(struct cbot_plugin *plugin,
                     const struct cbot_db_table *tbl);

/**
 * @brief Return the database connection.
 */
sqlite3 *cbot_db_conn(struct cbot *bot);

/******************
 * Logging API
 ******************/

enum cbot_log_level {
	VERB = 10,
	DEBUG = 20,
	INFO = 30,
	WARN = 40,
	CRIT = 50,
};

void cbot_log(int level, const char *format, ...);
void cbot_vlog(int level, const char *format, va_list args);

void cbot_set_log_level(int level);
int cbot_get_log_level(void);
void cbot_set_log_file(FILE *f);
int cbot_lookup_level(const char *str);

#define CL_CRIT(...)  cbot_log(CRIT, " CRIT: " __VA_ARGS__)
#define CL_WARN(...)  cbot_log(WARN, " WARN: " __VA_ARGS__)
#define CL_INFO(...)  cbot_log(INFO, " INFO: " __VA_ARGS__)
#define CL_DEBUG(...) cbot_log(DEBUG, "DEBUG: " __VA_ARGS__)
#define CL_VERB(...)  cbot_log(VERB, "VERB: " __VA_ARGS__)

#endif // CBOT_H
