/***************************************************************************//**

  @file         karma.c

  @author       Stephen Brennan

  @date         Created Sunday, 29 May 2016

  @brief        CBot "echo" handler to demonstrate regex capture support!

  @copyright    Copyright (c) 2016, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include "libstephen/re.h"
#include "cbot/cbot.h"

Regex r;

static void emote(cbot_event_t event, cbot_actions_t actions)
{
  size_t *captures = NULL;
  int incr = actions.addressed(event.bot, event.message);

  if (!incr)
    return;

  event.message += incr;

  if (reexec(r, event.message, &captures) == -1) {
    return;
  }

  Captures c = recap(event.message, captures, renumsaves(r));

  actions.me(event.bot, event.channel, c.cap[0]);

  recapfree(c);
}

void emote_load(cbot_t *bot, cbot_register_t registrar)
{
  r = recomp("emote (.*)");
  registrar(bot, CBOT_CHANNEL_MSG, emote);
}
