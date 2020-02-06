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

static int cbot_addressed(const struct cbot *bot, const char *message)
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

void cbot_init_handler_list(struct cbot_handler_list *list, size_t init_alloc)
{
	list->handler = smb_new(cbot_handler_t, init_alloc);
	list->num = 0;
	list->alloc = init_alloc;
}

void cbot_free_handler_list(struct cbot_handler_list *list)
{
	smb_free(list->handler);
}

/**
   @brief Create a cbot instance.

   A cbot instance is the heart of how cbot works.  In order to create one, you
   need to implement the functions that allow cbot to send on your backend
   (typically IRC, but also console).
   @param name The name of the cbot.
   @param send The "send" function.
   @return A new cbot instance.
 */
struct cbot *cbot_create(const char *name)
{
#define CBOT_INIT_ALLOC 32
	struct cbot *cbot = smb_new(struct cbot, 1);
	cbot->name = name;
	for (int i = 0; i < _CBOT_NUM_EVENT_TYPES_; i++) {
		cbot_init_handler_list(&cbot->hlists[i], CBOT_INIT_ALLOC);
	}
	cbot->actions.addressed = cbot_addressed;
	cbot->actions.is_authorized = cbot_is_authorized;
	OpenSSL_add_all_digests();
	return cbot;
}

/**
   @brief Free up all resources held by a cbot instance.
   @param cbot The bot to delete.
 */
void cbot_delete(struct cbot *cbot)
{
	for (int i = 0; i < _CBOT_NUM_EVENT_TYPES_; i++) {
		cbot_free_handler_list(&cbot->hlists[i]);
	}
	smb_free(cbot);
	EVP_cleanup();
}

static void cbot_add_to_handler_list(struct cbot_handler_list *list,
                                     cbot_handler_t handler)
{
	if (list->num >= list->alloc) {
		list->alloc *= 2;
		list->handler =
		        smb_renew(cbot_handler_t, list->handler, list->alloc);
	}
	list->handler[list->num] = handler;
	list->num++;
}

static struct cbot_handler_list *cbot_list_for_event(struct cbot *bot,
                                                     enum cbot_event_type type)
{
	return &bot->hlists[type];
}

/**
   Register an event handler for CBot!

   @param bot The bot instance to register into.
   @param type The type of event to register a handler for.
   @param handler Handler to register.
 */
void cbot_register(struct cbot *bot, enum cbot_event_type type,
                   cbot_handler_t handler)
{
	cbot_add_to_handler_list(cbot_list_for_event(bot, type), handler);
}

/**
   Dispatch an event for CBot.

   This goes through the correct handler list, looks for a matching handler, and
   calls the event handler. It doesn't terminate after the first event, because
   more than one plugin may want to see the event. So plugins have to make sure
   they don't get duplicate events accidentally.

   @param bot The bot we're working with.
   @param event The event to dispatch.
 */
void cbot_handle_event(struct cbot *bot, struct cbot_event event)
{
	struct cbot_handler_list *list = cbot_list_for_event(bot, event.type);

	for (size_t i = 0; i < list->num; i++) {
		cbot_handler_t handler = list->handler[i];
		handler(event, event.bot->actions);
	}
}

/**
   @brief Helper function that handles channel messages.

   CBot has the concept of messages directed AT the bot versus messages said in
   a channel, but not specifically targeted at the bot. IRC doesn't have this
   separation, so we have to separate out the two cases and then call the
   appropriate handlers.

   @param bot The bot we're operating on.
   @param channel The channel this message came in.
   @param user The user who said it.
   @param message The message itself.
 */
void cbot_handle_channel_message(struct cbot *bot, const char *channel,
                                 const char *user, const char *message)
{
	struct cbot_event event;

	event.bot = bot;
	event.type = CBOT_CHANNEL_MSG;
	event.channel = channel;
	event.username = user;
	event.message = message;

	cbot_handle_event(bot, event);
}

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
void cbot_load_plugins(struct cbot *bot, char *plugin_dir, smb_iter names)
{
	cbuf name;
	cbuf loader;
	smb_status status = SMB_SUCCESS;
	char *plugin_name;
	cb_init(&name, 256);
	cb_init(&loader, 256);

	while (names.has_next(&names)) {
		plugin_name = names.next(&names, &status).data_ptr;
		assert(status == SMB_SUCCESS);

		cb_clear(&name);
		cb_clear(&loader);

		// Construct a filename.
		cb_concat(&name, plugin_dir);
		if (plugin_dir[strlen(plugin_dir) - 1] != '/') {
			cb_append(&name, '/');
		}
		cb_concat(&name, plugin_name);
		cb_concat(&name, ".so");

		// Construct the loader name
		cb_printf(&loader, "%s_load", plugin_name);

		cbot_load_plugin(bot, name.buf, loader.buf);
	}

	cb_destroy(&name);
	cb_destroy(&loader);
}
