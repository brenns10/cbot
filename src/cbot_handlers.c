/***************************************************************************//**

  @file         cbot_handlers.c

  @author       Stephen Brennan

  @date         Created Thursday, 23 July 2015

  @brief        Some test CBot handlers.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include "cbot/cbot.h"

void cbot_hello(cbot_t *bot, const char *channel, const char *user, const char *message)
{
  cbot_send(bot, channel, "hello, %s!", user);
}

void cbot_handlers_register(cbot_t *bot)
{
  cbot_register_hear(bot, "(hello|hi),? cbot!?", cbot_hello);
}
