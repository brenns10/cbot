/***************************************************************************//**

  @file         cbot_handlers.c

  @author       Stephen Brennan

  @date         Created Thursday, 23 July 2015

  @brief        Some test CBot handlers.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <string.h>

#include "libstephen/re.h"
#include "cbot/cbot.h"

Regex greeting;

static void cbot_hello(cbot_event_t event, cbot_actions_t actions)
{
  if (reexec(greeting, event.message, NULL) == -1)
    return;
  actions.send(event.bot, event.channel, "hello, %s!", event.username);
}

void greet_load(cbot_t *bot, cbot_register_t registrar)
{
  greeting = recomp("[Hh](ello|i|ey),? +[Cc][Bb]ot!?");
  registrar(bot, CBOT_CHANNEL_MSG, cbot_hello);
}
