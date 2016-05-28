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

/**
   @brief Create a cbot instance.

   A cbot instance is the heart of how cbot works.  In order to create one, you
   need to implement the functions that allow cbot to send on your backend
   (typically IRC, but also console).
   @param name The name of the cbot.
   @param send The "send" function.
   @return A new cbot instance.
 */
cbot_t *cbot_create(const char *name, cbot_send_t send)
{
  #define CBOT_INIT_ALLOC 32
  cbot_t *cbot = smb_new(cbot_t, 1);
  cbot->name = name;

  cbot->hear_num = 0;
  cbot->hear_alloc = CBOT_INIT_ALLOC;
  cbot->hear_regex = smb_new(Regex, CBOT_INIT_ALLOC);
  cbot->hear_callback = smb_new(cbot_callback_t, CBOT_INIT_ALLOC);

  cbot->respond_num = 0;
  cbot->respond_alloc = CBOT_INIT_ALLOC;
  cbot->respond_regex = smb_new(Regex, CBOT_INIT_ALLOC);
  cbot->respond_callback = smb_new(cbot_callback_t, CBOT_INIT_ALLOC);

  cbot->send = send;
  return cbot;
}

/**
   @brief Free up all resources held by a cbot instance.
   @param cbot The bot to delete.
 */
void cbot_delete(cbot_t *cbot)
{
  for (int i = 0; i < cbot->hear_num; i++) {
    free_prog(cbot->hear_regex[i]);
  }
  smb_free(cbot->hear_regex);
  smb_free(cbot->hear_callback);

  for (int i = 0; i < cbot->respond_num; i++) {
    free_prog(cbot->respond_regex[i]);
  }
  smb_free(cbot->respond_regex);
  smb_free(cbot->respond_callback);

  smb_free(cbot);
}

static void cbot_register(Regex **regex_list, cbot_callback_t **callback_list,
                          int *regex_num, int *regex_alloc,
                          const char *regex, cbot_callback_t callback)
{
  if (*regex_num >= *regex_alloc) {
    *regex_alloc *= 2;
    *regex_list = smb_renew(Regex, *regex_list, *regex_alloc);
    *callback_list = smb_renew(cbot_callback_t, *callback_list, *regex_alloc);
  }
  (*regex_list)[*regex_num] = recomp(regex);
  (*callback_list)[*regex_num] = callback;
  *regex_num += 1;
}

/**
   @brief Register a callback for a regex that matches any message.
   @param bot The bot to register with.
   @param regex The regular expression the message should match.
   @param callback Callback function to run on a match.
 */
void cbot_register_hear(cbot_t *bot, const char *regex, cbot_callback_t callback)
{
  cbot_register(&bot->hear_regex, &bot->hear_callback, &bot->hear_num,
                &bot->hear_alloc, regex, callback);
}

/**
   @brief Register a callback for a regex that matches messages pointed at cbot.
   @param bot The bot to register with.
   @param regex The regular expression the message should match.
   @param callback Callback function to run on a match.
 */
void cbot_register_respond(cbot_t *bot, const char *regex, cbot_callback_t callback)
{
  cbot_register(&bot->respond_regex, &bot->respond_callback, &bot->respond_num,
                &bot->respond_alloc, regex, callback);
}

/**
   @brief Compare a regex against a message, and if it matches, call a callback.

   This does the plumbing of getting the number of captures in a form that's not
   a smb_al (because plugin code really shouldn't have to depend on libstephen).
   It does the regex simulation and everything.
   @param bot The bot we're working with.
   @param channel The channel the message came in on.
   @param user The user that sent the message.
   @param message The message!
   @param wmessage The message, in `wchar_t*`.
   @param f The regex FSM to match against the message.
   @param c The callback to call if there is a match.
   @param increment How many characters into the message should we start
   matching?
 */
static void cbot_match_callback(cbot_t *bot, const char *channel,
                                const char *user, const char *message, Regex r,
                                cbot_callback_t c, size_t increment)
{
  int i;
  size_t *saves = NULL, *starts = NULL, *ends = NULL;
  ssize_t  ncaptures;

  ssize_t match = execute(r, message + increment, &saves);
  if (match == -1) {
    free(saves);
    return;
  }

  ncaptures = numsaves(r);

  if (ncaptures != 0) {
    starts = smb_new(size_t, ncaptures);
    ends = smb_new(size_t, ncaptures);
    for (i = 0; i < ncaptures/2; i += 2) {
      starts[i] = saves[i*2] + increment;
      ends[i] = saves[i*2+1] + increment;
    }
    free(saves);
    c(bot, channel, user, message, starts, ends, ncaptures);
    smb_free(starts);
    smb_free(ends);
  } else {
    c(bot, channel, user, message, starts, ends, ncaptures);
  }
}

/**
   @brief Take an incoming message and call appropriate callbacks.
 */
void cbot_handle_message(cbot_t *bot, const char *channel, const char *user,
                         const char *message)
{
  size_t increment;

  // Handle the "hear" regex
  for (int i = 0; i < bot->hear_num; i++) {
    cbot_match_callback(bot, channel, user, message, bot->hear_regex[i],
                        bot->hear_callback[i], 0);
  }

  // If the message starts with the bot name, get the rest of it and match it
  // against the respond patterns and callbacks.
  increment = strlen(bot->name);
  if (strncmp(bot->name, message, increment) == 0) {
    while (isspace(message[increment]) || ispunct(message[increment])) {
      increment++;
    }

    for (int i = 0; i < bot->respond_num; i++) {
      cbot_match_callback(bot, channel, user, message, bot->respond_regex[i],
                          bot->respond_callback[i], increment);
    }
  }
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

  plugin(bot, cbot_register_hear, cbot_register_respond, bot->send);
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
