/***************************************************************************//**

  @file         sadness.c

  @author       Stephen Brennan

  @date         Created Tuesday, 28 July 2015

  @brief        CBot plugin to respond to hatred and sadness.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <stdlib.h>

#include "libstephen/re.h"
#include "cbot/cbot.h"

Regex r;
char *responses[] = {
  ":(",
  "hey, robots have feelings too!",
  "I don't like you, %s",
  "that's not what your mom said last night",
  "http://foaas.com/field/cbot/%s/Bible",
  "http://foaas.com/you/%s/cbot",
  "http://foaas.com/yoda/%s/cbot",
  "http://foaas.com/nugget/%s/cbot",
  "http://foaas.com/linus/%s/cbot",
  "http://foaas.com/shakespeare/%s/cbot",
  "http://foaas.com/donut/%s/cbot",
  "http://foaas.com/shutup/%s/cbot",
  "http://foaas.com/family/cbot",
  "http://foaas.com/cool/cbot",
  "http://foaas.com/everyone/cbot",
  "http://foaas.com/fascinating/cbot",
  "http://foaas.com/flying/cbot",
  "http://foaas.com/bucket/cbot"
};

static void sadness(cbot_event_t event, cbot_actions_t actions)
{
  int incr = actions.addressed(event.message, event.bot);
  if (!incr)
    return;

  if (reexec(r, event.message + incr, NULL) == -1)
    return;

  actions.send(event.bot, event.channel,
               responses[rand()%(sizeof(responses)/sizeof(char*))],
               event.username);
}

void sadness_load(cbot_t *bot, cbot_register_t registrar)
{
  r = recomp("([Yy]ou +[Ss]uck[!.]?|"
             "[Ss]ucks[!.]?|"
             "[Ii] +[Hh]ate +[Yy]ou[!.]?|"
             "[Ss]hut [Uu]p[!.]?)");
  registrar(bot, CBOT_CHANNEL_MSG, sadness);
}
