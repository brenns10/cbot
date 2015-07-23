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

#include "libstephen/al.h"
#include "libstephen/ht.h"

#include "cbot/cbot.h"

typedef void(*cbot_send_t)(cbot_t*, const char*, va_list);

struct cbot {

  const char *name;

  smb_al hear_regex;
  smb_al hear_callback;
  smb_al respond_regex;
  smb_al respond_callback;

  cbot_send_t send;
};

cbot_t *cbot_create(const char *name, cbot_send_t send);
void cbot_delete(cbot_t *obj);

#endif//CBOT_PRIVATE_H
