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
  "I don't like you, %s",
  "http://foaas.com/field/cbot/%s/Bible",
  "http://foaas.com/yoda/%s/cbot",
  "http://foaas.com/shutup/%s/cbot",
  "http://foaas.com/cool/cbot",
  "http://foaas.com/fascinating/cbot",
  "http://foaas.com/thumbs/This%20guy/cbot",
  "http://foaas.com/single/cbot",
  "http://foaas.com/pulp/C/cbot",
  "http://foaas.com/horse/cbot",
  "http://foaas.com/too/cbot",
  "http://foaas.com/outside/%s/cbot",
  "http://foaas.com/thanks/cbot",
  "http://foaas.com/madison/%s/cbot",
  "http://foaas.com/flying/cbot"
};

static void sadness(cbot_event_t event, cbot_actions_t actions)
{
  int incr = actions.addressed(event.bot, event.message);
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
