/***************************************************************************//**

  @file         cbot.c

  @author       Stephen Brennan

  @date         Created Wednesday, 22 July 2015

  @brief        CBot Implementation (only bot stuff, no IRC specifics).

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>

#include "libstephen/base.h"
#include "libstephen/al.h"
#include "libstephen/cb.h"
#include "libstephen/re.h"

#include "cbot_private.h"

void cbot_init_handler_list(cbot_handler_list_t *list, size_t init_alloc)
{
  list->regex = smb_new(Regex, init_alloc);
  list->handler = smb_new(cbot_handler_t, init_alloc);
  list->num = 0;
  list->alloc = init_alloc;
}

void cbot_free_handler_list(cbot_handler_list_t *list)
{
  for (size_t i = 0; i < list->num; i++) {
    free_prog(list->regex[i]);
  }
  smb_free(list->regex);
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
cbot_t *cbot_create(const char *name)
{
  #define CBOT_INIT_ALLOC 32
  cbot_t *cbot = smb_new(cbot_t, 1);
  cbot->name = name;
  cbot_init_handler_list(&cbot->hear, CBOT_INIT_ALLOC);
  cbot_init_handler_list(&cbot->msg, CBOT_INIT_ALLOC);
  return cbot;
}

/**
   @brief Free up all resources held by a cbot instance.
   @param cbot The bot to delete.
 */
void cbot_delete(cbot_t *cbot)
{
  cbot_free_handler_list(&cbot->hear);
  cbot_free_handler_list(&cbot->msg);
  smb_free(cbot);
}

static void cbot_add_to_handler_list(cbot_handler_list_t *list,
                                     const char *regex,
                                     cbot_handler_t handler)
{
  if (list->num >= list->alloc) {
    list->alloc *= 2;
    list->regex = smb_renew(Regex, list->regex, list->alloc);
    list->handler = smb_renew(cbot_handler_t, list->handler, list->alloc);
  }
  list->regex[list->num] = recomp(regex);
  list->handler[list->num] = handler;
  list->num++;
}

static cbot_handler_list_t *cbot_list_for_event(cbot_t *bot, cbot_event_type_t type)
{
  switch (type) {
  case CBOT_CHANNEL_MSG:
    return &bot->msg;
    break;
  case CBOT_CHANNEL_HEAR:
    return &bot->hear;
    break;
  }
}

/**
   Register an event handler for CBot!

   @param bot The bot instance to register into.
   @param type The type of event to register a handler for.
   @param regex Regex to match for the event.
   @param handler Handler to register.
 */
void cbot_register(cbot_t *bot, cbot_event_type_t type,
                          const char *regex, cbot_handler_t handler)
{
  cbot_add_to_handler_list(cbot_list_for_event(bot, type), regex, handler);
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
static void cbot_send_event(cbot_t *bot, cbot_event_t event)
{
  cbot_handler_list_t *list = cbot_list_for_event(bot, event.type);

  for (size_t i = 0; i < list->num; i++) {
    size_t *saves = NULL, *starts = NULL, *ends = NULL;
    Regex regex = list->regex[i];
    cbot_handler_t handler = list->handler[i];

    // Check for regex match.
    ssize_t match = execute(regex, event.message, &saves);
    if (match == -1) {
      free(saves);
      continue;
    }

    // Figure out how many captures we have.
    event.num_captures = numsaves(regex);

    if (event.num_captures != 0) {
      // If we have any, we need to split them into two lists.
      starts = smb_new(size_t, event.num_captures);
      ends = smb_new(size_t, event.num_captures);
      for (i = 0; i < event.num_captures/2; i += 2) {
        starts[i] = saves[i*2];
        ends[i] = saves[i*2+1];
      }
      free(saves);
      event.capture_starts = starts;
      event.capture_ends = ends;
      handler(event, bot->actions);
      smb_free(starts);
      smb_free(ends);
    } else {
      // Otherwise, just call the handler.
      handler(event, bot->actions);
    }
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
void cbot_handle_channel_message(cbot_t *bot, const char *channel,
                                 const char *user, const char *message)
{
  size_t increment = strlen(bot->name);
  cbot_event_t event;

  event.bot = bot;
  event.channel = channel;
  event.username = user;
  event.message = message;
  event.num_captures = 0;

  // Check if the message starts with the bot's name.
  if (strncmp(bot->name, message, increment) == 0) {
    // If so, skip whitespace and punctuation until the rest of the message.
    while (isspace(message[increment]) || ispunct(message[increment])) {
      increment++;
    }

    // Adjust the message and say this was a MSG event.
    event.message += increment;
    event.type = CBOT_CHANNEL_MSG;
  } else {
    event.type = CBOT_CHANNEL_HEAR;
  }

  cbot_send_event(bot, event);
}

/**
   @brief Private function to load a single plugin.
 */
static bool cbot_load_plugin(cbot_t *bot, const char *filename, const char *loader)
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

  plugin(bot, cbot_register);
  return true;
}

/**
   @brief Load a list of plugins from a plugin directory.
 */
void cbot_load_plugins(cbot_t *bot, char *plugin_dir, smb_iter names)
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
    if (plugin_dir[strlen(plugin_dir)-1] != '/') {
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
