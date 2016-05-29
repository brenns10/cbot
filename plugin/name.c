/***************************************************************************//**

  @file         name.c

  @author       Stephen Brennan

  @date         Created Thursday, 23 July 2015

  @brief        CBot plugin to respond to questions about cbot.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include "cbot/cbot.h"


static void name(cbot_event_t event, cbot_actions_t actions)
{
  actions.send(event.bot, event.channel, "My name is CBot.  My source lives at https://github.com/brenns10/cbot");
}

void name_load(cbot_t *bot, cbot_register_t registrar)
{
  registrar(bot, CBOT_CHANNEL_HEAR,
            "([wW]ho|[wW]hat|[wW][tT][fF]) +([iI]s|[aA]re +[yY]ou,?) +[cC][bB]ot\\??",
            name);
}
