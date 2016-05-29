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

typedef struct {
  Regex *regex;
  cbot_handler_t *handler;
  size_t num;
  size_t alloc;
} cbot_handler_list_t;

struct cbot {

  const char *name;

  cbot_handler_list_t hear;
  cbot_handler_list_t msg;

  void *backend;

  cbot_actions_t actions;
};

cbot_t *cbot_create(const char *name);
void cbot_delete(cbot_t *obj);
void cbot_handle_event(cbot_event_t event);
void cbot_handle_channel_message(cbot_t *bot, const char *channel,
                                 const char *user, const char *message);
void cbot_register(cbot_t *bot, cbot_event_type_t type, const char *regex,
                   cbot_handler_t handler);
void cbot_load_plugins(cbot_t *bot, char *plugin_dir, smb_iter names);

#endif//CBOT_PRIVATE_H
