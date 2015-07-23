/***************************************************************************//**

  @file         cbot_handlers.c

  @author       Stephen Brennan

  @date         Created Thursday, 23 July 2015

  @brief        Some test CBot handlers.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include "cbot/cbot.h"

static cbot_send_t send;

static void cbot_hello(cbot_t *bot, const char *channel, const char *user, const char *message)
{
  send(bot, channel, "hello, %s!", user);
}

void greet_load(cbot_t *bot, cbot_register_t hear, cbot_register_t respond, cbot_send_t send_)
{
  send = send_;
  hear(bot, "(hello|hi),? cbot!?", cbot_hello);
}
