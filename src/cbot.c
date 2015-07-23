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
#include <dlfcn.h>

#include "libstephen/base.h"
#include "libstephen/al.h"
#include "libstephen/cb.h"
#include "libstephen/regex.h"

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
  cbot_t *cbot = smb_new(cbot_t, 1);
  cbot->name = name;
  al_init(&cbot->hear_regex);
  al_init(&cbot->hear_callback);
  al_init(&cbot->respond_regex);
  al_init(&cbot->respond_callback);
  cbot->send = send;
  return cbot;
}

/**
   @brief Free up all resources held by a cbot instance.
   @param cbot The bot to delete.
 */
void cbot_delete(cbot_t *cbot)
{
  // TODO - free the regexes before deleting them.
  al_destroy(&cbot->hear_regex);
  al_destroy(&cbot->hear_callback);
  al_destroy(&cbot->respond_regex);
  al_destroy(&cbot->respond_callback);
  smb_free(cbot);
}

/**
   @brief Send a message through the cbot!
   @param bot The bot to use.
   @param dest The channel (prefixed with '#') or user (no prefix) to send to.
   @param format Format string for your message.
   @param ... Variables for formatting.
 */
void cbot_send(cbot_t *bot, const char *dest, char *format, ...)
{
}

static void cbot_register(smb_al *regex_list, smb_al *callback_list,
                          const char *regex, cbot_callback_t callback)
{
  fsm *re;
  int nchar = mbstowcs(NULL, regex, 0);
  wchar_t *wregex = smb_new(wchar_t, nchar+1);
  mbstowcs(wregex, regex, nchar+1);
  re = regex_parse(wregex);
  smb_free(wregex);
  al_append(regex_list, (DATA){.data_ptr=re});
  al_append(callback_list, (DATA){.data_ptr=callback});
}

/**
   @brief Register a callback for a regex that matches any message.
   @param bot The bot to register with.
   @param regex The regular expression the message should match.
   @param callback Callback function to run on a match.
 */
void cbot_register_hear(cbot_t *bot, const char *regex, cbot_callback_t callback)
{
  cbot_register(&bot->hear_regex, &bot->hear_callback, regex, callback);
}

/**
   @brief Register a callback for a regex that matches messages pointed at cbot.
   @param bot The bot to register with.
   @param regex The regular expression the message should match.
   @param callback Callback function to run on a match.
 */
void cbot_register_respond(cbot_t *bot, const char *regex, cbot_callback_t callback)
{
  cbot_register(&bot->hear_regex, &bot->hear_callback, regex, callback);
}

/**
   @brief Take an incoming message and call appropriate callbacks.
 */
void cbot_handle_message(cbot_t *bot, const char *channel, const char *user,
                         const char *message)
{
  smb_iter re, cb;
  fsm *f;
  cbot_callback_t c;
  smb_status status = SMB_SUCCESS;
  size_t nchars;
  wchar_t *wch;

  nchars = mbstowcs(NULL, message, 0);
  wch = smb_new(wchar_t, nchars + 1);
  mbstowcs(wch, message, nchars + 1);

  // Handle the "hear" regex
  re = al_get_iter(&bot->hear_regex);
  cb = al_get_iter(&bot->hear_callback);
  while (re.has_next(&re)) {
    f = re.next(&re, &status).data_ptr;
    assert(status == SMB_SUCCESS);
    c = cb.next(&cb, &status).data_ptr;
    assert(status == SMB_SUCCESS);
    if (fsm_sim_nondet(f, wch)) {
      c(bot, channel, user, message);
    }
  }

  // TODO - handle the respond regex

  smb_free(wch);
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
void cbot_load_plugins(cbot_t *bot, char *plugin_dir, smb_al *names)
{
  cbuf name;
  cbuf loader;
  smb_status status = SMB_SUCCESS;
  smb_iter it;
  char *plugin_name;
  cb_init(&name, 256);
  cb_init(&loader, 256);

  it = al_get_iter(names);
  while (it.has_next(&it)) {
    plugin_name = it.next(&it, &status).data_ptr;
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
