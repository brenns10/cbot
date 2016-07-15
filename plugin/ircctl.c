/***************************************************************************//**

  @file         ircctl.c

  @author       Stephen Brennan

  @date         Created Sunday, 14 July 2016

  @brief        Ask cbot to do things on IRC for you.

  @copyright    Copyright (c) 2016, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include "libstephen/re.h"
#include "cbot/cbot.h"

#define HASH "([A-Za-z0-9+=/]+)"

#define NCMD 2

Regex commands[NCMD];
void (*handlers[NCMD])(cbot_event_t, cbot_actions_t, Captures);

static void op_handler(cbot_event_t event, cbot_actions_t actions, Captures c) {
  actions.op(event.bot, event.channel, c.cap[0]);
}

static void join_handler(cbot_event_t event, cbot_actions_t actions, Captures c) {
  actions.join(event.bot, c.cap[0], NULL);
}

static void handler(cbot_event_t event, cbot_actions_t actions)
{
  size_t *captures = NULL;
  Captures c;
  int incr = actions.addressed(event.bot, event.message);

  if (!incr)
    return;
  event.message += incr;

  for (int idx = 0; idx < NCMD; idx++) {
    if (reexec(commands[idx], event.message, &captures) == -1) {
      continue;
    }
    c = recap(event.message, captures, renumsaves(commands[idx]));
    if (actions.is_authorized(event.bot, c.cap[c.n-1])) {
      handlers[idx](event, actions, c);
    } else {
      actions.send(event.bot, event.channel, "sorry, you aren't authorized to do that!");
    }
    recapfree(c);
  }
}

void ircctl_load(cbot_t *bot, cbot_register_t registrar)
{
  //commands[0] = recomp("join +(.*) " HASH);
  commands[0] = recomp("op +(.*) +" HASH);
  handlers[0] = op_handler;

  commands[1] = recomp("join +(.*) +" HASH);
  handlers[1] = join_handler;
  //invite = recomp(" invite +(.*) +(.*) " HASH);
  registrar(bot, CBOT_CHANNEL_MSG, handler);
}
