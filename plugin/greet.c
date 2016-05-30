/***************************************************************************//**

  @file         cbot_handlers.c

  @author       Stephen Brennan

  @date         Created Thursday, 23 July 2015

  @brief        Some test CBot handlers.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <string.h>

#include "cbot/cbot.h"

static void cbot_hello(cbot_event_t event, cbot_actions_t actions)
{
  actions.send(event.bot, event.channel, "hello, %s!", event.username);
}

static void cbot_goodbye(cbot_event_t event, cbot_actions_t actions)
{
  actions.send(event.bot, event.channel, "good riddance!");
}

void greet_load(cbot_t *bot, cbot_register_t registrar)
{
  registrar(bot, CBOT_CHANNEL_HEAR,
            "[Hh](ello|i|ey),? +[Cc][Bb]ot!?", cbot_hello);
  registrar(bot, CBOT_JOIN, "h", cbot_hello);
  registrar(bot, CBOT_PART, "h", cbot_goodbye);
}
