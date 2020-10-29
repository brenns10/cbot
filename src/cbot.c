/**
 * cbot.c: core CBot implementation
 */

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>
#include <sc-collections.h>
#include <sqlite3.h>

#include "cbot_private.h"

/********
 * Functions which plugins can call to perform actions. These are generally
 * delegated to the backends.
 ********/

void cbot_send(const struct cbot *cbot, const char *dest, const char *format,
               ...)
{
	va_list va;
	struct sc_charbuf cb;
	va_start(va, format);
	sc_cb_init(&cb, 1024);
	sc_cb_vprintf(&cb, (char *)format, va);
	cbot->backend->send(cbot, dest, cb.buf);
	sc_cb_destroy(&cb);
	va_end(va);
}

void cbot_me(const struct cbot *cbot, const char *dest, const char *format, ...)
{
	va_list va;
	struct sc_charbuf cb;
	va_start(va, format);
	sc_cb_init(&cb, 1024);
	sc_cb_vprintf(&cb, (char *)format, va);
	cbot->backend->me(cbot, dest, cb.buf);
	sc_cb_destroy(&cb);
	va_end(va);
}

void cbot_op(const struct cbot *cbot, const char *channel, const char *person)
{
	cbot->backend->op(cbot, channel, person);
}

void cbot_join(const struct cbot *cbot, const char *channel,
               const char *password)
{
	cbot->backend->join(cbot, channel, password);
}

void cbot_nick(const struct cbot *cbot, const char *newnick)
{
	cbot->backend->nick(cbot, newnick);
}

int cbot_addressed(const struct cbot *bot, const char *message)
{
	int increment = strlen(bot->name);
	if (strncmp(bot->name, message, increment) == 0) {
		while (isspace(message[increment]) ||
		       ispunct(message[increment])) {
			increment++;
		}
		return increment;
	}
	return 0;
}

/********
 * Functions related to the SQL private DB
 ********/

int cbot_db_create_tables(struct cbot *bot)
{
	int rv;
	char *errmsg = NULL;
	char *stmts = "CREATE TABLE user ( "
	              " id INTEGER PRIMARY KEY ASC, "
	              " nick TEXT NOT NULL UNIQUE, "
	              " realname TEXT, "
	              " host TEXT "
	              "); "
	              "CREATE TABLE channel ( "
	              " id INTEGER PRIMARY KEY ASC, "
	              " name TEXT NOT NULL UNIQUE, "
	              " topic TEXT "
	              "); "
	              "CREATE TABLE membership ( "
	              " user_id INT NOT NULL, "
	              " channel_id INT NOT NULL, "
	              " UNIQUE(user_id, channel_id) "
	              "); ";
	rv = sqlite3_exec(bot->privDb, stmts, NULL, NULL, &errmsg);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "sqlite error creating tables: %s\n", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	return 0;
}

int cbot_db_get_user_id(struct cbot *bot, char *nick)
{
	char *select = "SELECT id FROM user WHERE nick=?;";
	sqlite3_stmt *stmt = NULL;
	int rv, user_id;

	rv = sqlite3_prepare_v2(bot->privDb, select, -1, &stmt, NULL);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "prepare: %s(%d): %s\n", sqlite3_errstr(rv), rv,
		        sqlite3_errmsg(bot->privDb));
		return -1;
	}

	rv = sqlite3_bind_text(stmt, 1, nick, -1, SQLITE_STATIC);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "bind: %d\n", rv);
		return -1;
	}

	rv = sqlite3_step(stmt);
	if (rv == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return 0;
	} else if (rv == SQLITE_ROW) {
		user_id = sqlite3_column_int(stmt, 0);
	} else {
		fprintf(stderr, "step: %d\n", rv);
		sqlite3_finalize(stmt);
		return -1;
	}

	rv = sqlite3_step(stmt);
	if (rv != SQLITE_DONE) {
		fprintf(stderr, "step gave more data %d\n", rv);
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_finalize(stmt);
	return user_id;
}

int cbot_db_insert_user(struct cbot *bot, char *nick)
{
	char *select = "INSERT INTO user(nick) VALUES (?);";
	sqlite3_stmt *stmt = NULL;
	int rv;

	rv = sqlite3_prepare_v2(bot->privDb, select, -1, &stmt, NULL);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "prepare: %s(%d): %s\n", sqlite3_errstr(rv), rv,
		        sqlite3_errmsg(bot->privDb));
		return -1;
	}

	rv = sqlite3_bind_text(stmt, 1, nick, -1, SQLITE_STATIC);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "bind: %d\n", rv);
		return -1;
	}

	rv = sqlite3_step(stmt);
	if (rv == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return (int)sqlite3_last_insert_rowid(bot->privDb);
	} else {
		sqlite3_finalize(stmt);
		fprintf(stderr, "step: %d\n", rv);
	}
	return -1;
}

int cbot_db_upsert_user(struct cbot *bot, char *nick)
{
	int user_id = cbot_db_get_user_id(bot, nick);
	if (user_id > 0)
		return user_id;

	int rv = cbot_db_insert_user(bot, nick);
	return rv;
}

int cbot_db_get_chan(struct cbot *bot, char *chan)
{
	char *select = "SELECT id FROM channel WHERE name=?;";
	sqlite3_stmt *stmt = NULL;
	int rv, chan_id;

	rv = sqlite3_prepare_v2(bot->privDb, select, -1, &stmt, NULL);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "prepare: %s(%d): %s\n", sqlite3_errstr(rv), rv,
		        sqlite3_errmsg(bot->privDb));
		return -1;
	}

	rv = sqlite3_bind_text(stmt, 1, chan, -1, SQLITE_STATIC);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "bind: %d\n", rv);
		return -1;
	}

	rv = sqlite3_step(stmt);
	if (rv == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return 0;
	} else if (rv == SQLITE_ROW) {
		chan_id = sqlite3_column_int(stmt, 0);
	} else {
		fprintf(stderr, "step: %d\n", rv);
		sqlite3_finalize(stmt);
		return -1;
	}

	rv = sqlite3_step(stmt);
	if (rv != SQLITE_DONE) {
		fprintf(stderr, "step gave more data %d\n", rv);
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_finalize(stmt);
	return chan_id;
}

int cbot_db_insert_chan(struct cbot *bot, char *name)
{
	char *select = "INSERT INTO channel(name) VALUES (?);";
	sqlite3_stmt *stmt = NULL;
	int rv;

	rv = sqlite3_prepare_v2(bot->privDb, select, -1, &stmt, NULL);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "prepare: %s(%d): %s\n", sqlite3_errstr(rv), rv,
		        sqlite3_errmsg(bot->privDb));
		return -1;
	}

	rv = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "bind: %d\n", rv);
		return -1;
	}

	rv = sqlite3_step(stmt);
	if (rv == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return (int)sqlite3_last_insert_rowid(bot->privDb);
	} else {
		sqlite3_finalize(stmt);
		fprintf(stderr, "step: %d\n", rv);
	}
	return -1;
}

int cbot_db_upsert_chan(struct cbot *bot, char *name)
{
	int chan_id = cbot_db_get_chan(bot, name);
	if (chan_id > 0)
		return chan_id;

	chan_id = cbot_db_insert_chan(bot, name);
	return chan_id;
}

int cbot_db_upsert_membership(struct cbot *bot, int user_id, int chan_id)
{
	char *insert =
	        "INSERT INTO membership(user_id, channel_id) VALUES(?, ?) "
	        "ON CONFLICT DO NOTHING;";
	sqlite3_stmt *stmt = NULL;
	int rv;

	rv = sqlite3_prepare_v2(bot->privDb, insert, -1, &stmt, NULL);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "prepare: %s(%d): %s\n", sqlite3_errstr(rv), rv,
		        sqlite3_errmsg(bot->privDb));
		return -1;
	}

	rv = sqlite3_bind_int(stmt, 1, user_id);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "bind: %d\n", rv);
		return -1;
	}
	rv = sqlite3_bind_int(stmt, 2, chan_id);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "bind: %d\n", rv);
		return -1;
	}

	rv = sqlite3_step(stmt);
	if (rv == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return 0;
	} else {
		sqlite3_finalize(stmt);
		fprintf(stderr, "step: %d\n", rv);
		return -1;
	}
}

static int cbot_db_cb_add_user(void *ptr, int cols, char **values, char **names)
{
	struct sc_list_head *head = (struct sc_list_head *)ptr;
	struct cbot_user_info *info = malloc(sizeof(struct cbot_user_info));
	info->username = strdup(values[0]);
	info->realname = NULL;
	sc_list_insert_end(head, &info->list);
	return 0;
}

void cbot_user_info_free(struct cbot_user_info *info)
{
	free(info->username);
	if (info->realname)
		free(info->realname);
	free(info);
}

void cbot_user_info_free_all(struct sc_list_head *head)
{
	struct cbot_user_info *info, *next;
	sc_list_for_each_safe(info, next, head, list, struct cbot_user_info)
	{
		cbot_user_info_free(info);
	}
}

int cbot_get_members(struct cbot *bot, char *chan, struct sc_list_head *head)
{
	char *cmd = "SELECT u.nick "
	            "FROM user u "
	            " INNER JOIN membership m ON u.id=m.user_id "
	            " INNER JOIN channel c ON c.id=m.channel_id "
	            "WHERE c.name = \"%s\" "
	            ";";
	char *formatted;
	int rv;
	struct sc_list_head tmp;
	struct sc_list_head *it;

	sc_list_init(&tmp);
	rv = snprintf(NULL, 0, cmd, chan);
	formatted = malloc(rv + 1);
	sprintf(formatted, cmd, chan);
	rv = sqlite3_exec(bot->privDb, formatted, cbot_db_cb_add_user, &tmp,
	                  NULL);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "exec: %s(%d): %s\n", sqlite3_errstr(rv), rv,
		        sqlite3_errmsg(bot->privDb));
		cbot_user_info_free_all(&tmp);
		free(formatted);
		return -1;
	}
	free(formatted);
	rv = 0;
	sc_list_for_each(it, &tmp)
	{
		rv++;
	}
	sc_list_move(&tmp, head);
	return rv;
}

int cbot_add_membership(struct cbot *bot, char *nick, char *chan)
{
	int user_id = cbot_db_upsert_user(bot, nick);
	int chan_id = cbot_db_upsert_chan(bot, chan);
	int rv;
	if (user_id < 0 || chan_id < 0)
		return -1;
	rv = cbot_db_upsert_membership(bot, user_id, chan_id);
	return rv;
}

/********
 * Functions related to the bot lifetime
 ********/

/**
   @brief Create a cbot instance.

   A cbot instance is the heart of how cbot works.  In order to create one, you
   need to implement the functions that allow cbot to send on your backend
   (typically IRC, but also console).
   @param name The name of the cbot.
   @return A new cbot instance.
 */
struct cbot *cbot_create(const char *name, struct cbot_backend *backend)
{
	struct cbot *cbot = malloc(sizeof(struct cbot));
	int rv;
	cbot->name = strdup(name);
	for (int i = 0; i < _CBOT_NUM_EVENT_TYPES_; i++) {
		sc_list_init(&cbot->handlers[i]);
	}
	OpenSSL_add_all_digests();
	cbot->backend = backend;
	rv = sqlite3_open_v2("cbot_priv", &cbot->privDb,
	                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
	                             SQLITE_OPEN_MEMORY,
	                     NULL);
	if (rv != SQLITE_OK) {
		sqlite3_close(cbot->privDb);
		free(cbot->name);
		free(cbot);
		cbot = NULL;
	}

	rv = cbot_db_create_tables(cbot);
	if (rv < 0) {
		sqlite3_close(cbot->privDb);
		free(cbot->name);
		free(cbot);
		cbot = NULL;
	}
	return cbot;
}

void cbot_set_nick(struct cbot *bot, const char *newname)
{
	struct cbot_nick_event event, copy;
	struct cbot_handler *hdlr, *next;

	event.bot = bot;
	event.old_username = bot->name;
	bot->name = strdup(newname);
	event.new_username = bot->name;
	event.type = CBOT_BOT_NAME;

	if (strcmp(event.old_username, event.new_username) != 0) {
		sc_list_for_each_safe(hdlr, next, &bot->handlers[event.type],
		                      handler_list, struct cbot_handler)
		{
			copy = event; /* safe in case of modification */
			hdlr->handler((struct cbot_event *)&copy, hdlr->user);
		}
	}

	free((char *)event.old_username);
}
const char *cbot_get_name(struct cbot *bot)
{
	return bot->name;
}

/**
   @brief Free up all resources held by a cbot instance.
   @param cbot The bot to delete.
 */
void cbot_delete(struct cbot *cbot)
{
	struct cbot_handler *hdlr, *next;
	for (int i = 0; i < _CBOT_NUM_EVENT_TYPES_; i++) {
		sc_list_for_each_safe(hdlr, next, &cbot->handlers[i],
		                      handler_list, struct cbot_handler)
		{
			cbot_deregister(cbot, hdlr);
		}
	}
	int rv = sqlite3_close(cbot->privDb);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "error closing sqlite db on shutdown\n");
	}
	free(cbot->name);
	free(cbot);
	EVP_cleanup();
}

/*********
 * Functions related to handlers: registration and handling of events
 *********/

/**
 * Register an event handler for CBot!
 * @param bot The bot instance to register into.
 * @param type The type of event to register a handler for.
 * @param handler Handler to register.
 */
struct cbot_handler *cbot_register(struct cbot *bot, enum cbot_event_type type,
                                   cbot_handler_t handler, void *user,
                                   char *regex)
{
	struct cbot_handler *hdlr = calloc(1, sizeof(*hdlr));
	hdlr->handler = handler;
	hdlr->user = user;
	if (regex)
		hdlr->regex = sc_regex_compile(regex);
	sc_list_insert_end(&bot->handlers[type], &hdlr->handler_list);
	sc_list_init(&hdlr->plugin_list); // TODO add when plugins implemented
	return hdlr;
}

void cbot_deregister(struct cbot *bot, struct cbot_handler *hdlr)
{
	sc_list_remove(&hdlr->handler_list);
	sc_list_remove(&hdlr->plugin_list);
	if (hdlr->regex) {
		sc_regex_free(hdlr->regex);
	}
	free(hdlr);
}

static void cbot_dispatch_msg(struct cbot *bot, struct cbot_message_event event,
                              enum cbot_event_type type)
{
	struct cbot_handler *hdlr;
	struct cbot_message_event copy;
	size_t *indices;
	int result;
	sc_list_for_each_entry(hdlr, &bot->handlers[type], handler_list,
	                       struct cbot_handler)
	{
		if (!hdlr->regex) {
			event.indices = NULL;
			event.num_captures = 0;
			copy = event; /* safe in case of modification */
			hdlr->handler((struct cbot_event *)&copy, hdlr->user);
		} else {
			result = sc_regex_exec(hdlr->regex, event.message,
			                       &indices);
			if (result != -1) {
				event.indices = indices;
				event.num_captures =
				        sc_regex_num_captures(hdlr->regex);
				copy = event;
				hdlr->handler((struct cbot_event *)&copy,
				              hdlr->user);
				free(indices);
			}
		}
	}
}

/**
 * @brief Function called by backends to handle standard messages
 *
 * CBot has the concept of messages directed AT the bot versus messages said in
 * a channel, but not specifically targeted at the bot. IRC doesn't have this
 * separation, so we have to separate out the two cases and then call the
 * appropriate handlers.
 *
 * IRC also has tho concept of "action" messages, e.g. /me says hello. This
 * function serves these messages too, using the "action" flag.
 *
 * @param bot The bot we're operating on
 * @param channel The channel this message came in
 * @param user The user who said it
 * @param message The message itself
 * @param action True if the message was a CTCP action, false otherwise.
 */
void cbot_handle_message(struct cbot *bot, const char *channel,
                         const char *user, const char *message, bool action)
{
	struct cbot_message_event event;
	int address_increment = cbot_addressed(bot, message);

	/* shared fields */
	event.bot = bot;
	event.channel = channel;
	event.username = user;
	event.is_action = action;
	event.indices = NULL;

	/* When cbot is directly addressed */
	if (address_increment) {
		event.message = message + address_increment;
		event.type = CBOT_ADDRESSED;
		cbot_dispatch_msg(bot, event, CBOT_ADDRESSED);
	}

	event.type = CBOT_MESSAGE;
	event.message = message;
	cbot_dispatch_msg(bot, event, CBOT_MESSAGE);
}

void cbot_handle_user_event(struct cbot *bot, const char *channel,
                            const char *user, enum cbot_event_type type)
{
	struct cbot_user_event event, copy;
	struct cbot_handler *hdlr;
	event.bot = bot;
	event.type = type;
	event.channel = channel;
	event.username = user;

	sc_list_for_each_entry(hdlr, &bot->handlers[type], handler_list,
	                       struct cbot_handler)
	{
		copy = event; /* safe in case of modification */
		hdlr->handler((struct cbot_event *)&copy, hdlr->user);
	}
}

void cbot_handle_nick_event(struct cbot *bot, const char *old_username,
                            const char *new_username)
{
	struct cbot_nick_event event, copy;
	struct cbot_handler *hdlr;
	event.bot = bot;
	event.type = CBOT_NICK;
	event.old_username = old_username;
	event.new_username = new_username;

	sc_list_for_each_entry(hdlr, &bot->handlers[CBOT_NICK], handler_list,
	                       struct cbot_handler)
	{
		copy = event; /* safe in case of modification */
		hdlr->handler((struct cbot_event *)&copy, hdlr->user);
	}
}

/**********
 * Functions related to plugins
 **********/

/**
   @brief Private function to load a single plugin.
 */
static bool cbot_load_plugin(struct cbot *bot, const char *filename,
                             const char *loader)
{
	void *plugin_handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);

	printf("attempting to load function %s from %s\n", loader, filename);

	if (plugin_handle == NULL) {
		fprintf(stderr, "cbot_load_plugin: %s\n", dlerror());
		return false;
	}

	cbot_plugin_t plugin = dlsym(plugin_handle, loader);

	if (plugin == NULL) {
		fprintf(stderr, "cbot_load_plugin: %s\n", dlerror());
		return false;
	}

	plugin(bot);
	return true;
}

/**
   @brief Load a list of plugins from a plugin directory.
 */
void cbot_load_plugins(struct cbot *bot, char *plugin_dir, char **names,
                       int count)
{
	struct sc_charbuf name;
	struct sc_charbuf loader;
	int i;
	sc_cb_init(&name, 256);
	sc_cb_init(&loader, 256);

	for (i = 0; i < count; i++) {
		sc_cb_clear(&name);
		sc_cb_clear(&loader);

		// Construct a filename.
		sc_cb_concat(&name, plugin_dir);
		if (plugin_dir[strlen(plugin_dir) - 1] != '/') {
			sc_cb_append(&name, '/');
		}
		sc_cb_concat(&name, names[i]);
		sc_cb_concat(&name, ".so");

		// Construct the loader name
		sc_cb_printf(&loader, "%s_load", names[i]);

		cbot_load_plugin(bot, name.buf, loader.buf);
	}

	sc_cb_destroy(&name);
	sc_cb_destroy(&loader);
}
