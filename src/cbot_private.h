/***************************************************************************//**

  @file         cbot_private.h

  @author       Stephen Brennan

  @date         Created Wednesday, 22 July 2015

  @brief        Declarations for CBot (implementation details).

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#ifndef CBOT_PRIVATE_H
#define CBOT_PRIVATE_H

#include <stdarg.h>

#include "libstephen/re.h"
#include "libstephen/list.h"

#include "cbot/cbot.h"

struct cbot {

  const char *name;

  Regex *hear_regex;
  cbot_callback_t *hear_callback;
  int hear_num;
  int hear_alloc;
  Regex *respond_regex;
  cbot_callback_t *respond_callback;
  int respond_num;
  int respond_alloc;

  void *backend;

  cbot_send_t send;
};

cbot_t *cbot_create(const char *name, cbot_send_t send);
void cbot_delete(cbot_t *obj);
void cbot_handle_message(cbot_t *bot, const char *channel, const char *user,
                         const char *message);
void cbot_register_hear(cbot_t *bot, const char *regex, cbot_callback_t callback);
void cbot_register_respond(cbot_t *bot, const char *regex, cbot_callback_t callback);
void cbot_load_plugins(cbot_t *bot, char *plugin_dir, smb_iter names);

#endif//CBOT_PRIVATE_H
