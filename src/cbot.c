/**
 * cbot.c: core CBot implementation
 */

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>

#include "libstephen/al.h"
#include "libstephen/base.h"
#include "libstephen/cb.h"

#include "cbot_private.h"

/********
 * Functions which plugins can call to perform actions. These are generally
 * delegated to the backends.
 ********/

void cbot_send(const struct cbot *cbot, const char *dest, const char *format,
               ...)
{
	va_list va;
	cbuf cb;
	va_start(va, format);
	cb_init(&cb, 1024);
	cb_vprintf(&cb, (char *)format, va);
	cbot->backend->send(cbot, dest, cb.buf);
	cb_destroy(&cb);
	va_end(va);
}

void cbot_me(const struct cbot *cbot, const char *dest, const char *format, ...)
{
	va_list va;
	cbuf cb;
	va_start(va, format);
	cb_init(&cb, 1024);
	cb_vprintf(&cb, (char *)format, va);
	cbot->backend->me(cbot, dest, cb.buf);
	cb_destroy(&cb);
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
	cbot->name = strdup(name);
	for (int i = 0; i < _CBOT_NUM_EVENT_TYPES_; i++) {
		sc_list_init(&cbot->handlers[i]);
	}
	OpenSSL_add_all_digests();
	cbot->backend = backend;
	return cbot;
}

void cbot_set_nick(struct cbot *bot, const char *newname)
{
	free(bot->name);
	bot->name = strdup(newname);
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
			if (hdlr->regex) {
				sc_regex_free(hdlr->regex);
			}
			sc_list_remove(&hdlr->handler_list);
			free(hdlr);
		}
	}
	smb_free(cbot);
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
void cbot_register(struct cbot *bot, enum cbot_event_type type,
                   cbot_handler_t handler, void *user, char *regex)
{
	struct cbot_handler *hdlr = malloc(sizeof(struct cbot_handler));
	hdlr->handler = handler;
	hdlr->user = user;
	if (regex)
		hdlr->regex = sc_regex_compile(regex);
	sc_list_insert_end(&bot->handlers[type], &hdlr->handler_list);
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
void cbot_load_plugins(struct cbot *bot, char *plugin_dir, char **names, int count)
{
	cbuf name;
	cbuf loader;
	char *plugin_name;
	int i;
	cb_init(&name, 256);
	cb_init(&loader, 256);

	for (i = 0; i < count; i++) {
		cb_clear(&name);
		cb_clear(&loader);

		// Construct a filename.
		cb_concat(&name, plugin_dir);
		if (plugin_dir[strlen(plugin_dir) - 1] != '/') {
			cb_append(&name, '/');
		}
		cb_concat(&name, names[i]);
		cb_concat(&name, ".so");

		// Construct the loader name
		cb_printf(&loader, "%s_load", names[i]);

		cbot_load_plugin(bot, name.buf, loader.buf);
	}

	cb_destroy(&name);
	cb_destroy(&loader);
}
