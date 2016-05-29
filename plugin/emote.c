/***************************************************************************//**

  @file         karma.c

  @author       Stephen Brennan

  @date         Created Sunday, 29 May 2016

  @brief        CBot "echo" handler to demonstrate regex capture support!

  @copyright    Copyright (c) 2016, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include "cbot/cbot.h"

static void emote(cbot_event_t event, cbot_actions_t actions)
{
  actions.me(event.bot, event.channel, "laughs");
}

void emote_load(cbot_t *bot, cbot_register_t registrar)
{
  registrar(bot, CBOT_CHANNEL_MSG, "emote (.*)", emote);
}
