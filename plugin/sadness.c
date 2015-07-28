/***************************************************************************//**

  @file         sadness.c

  @author       Stephen Brennan

  @date         Created Tuesday, 28 July 2015

  @brief        CBot plugin to respond to hatred and sadness.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <stdlib.h>
#include "cbot/cbot.h"

static cbot_send_t send;

static void sadness(cbot_t *bot, const char *channel, const char *user,
                    const char *message)
{
  char *responses[] = {
    ":(",
    "hey, robots have feelings too!",
    "I don't like you, %s",
    "that's not what your mom said last night"
  };
  send(bot, channel, responses[rand()%(sizeof(responses)/sizeof(char*))], user);
}

void sadness_load(cbot_t *bot, cbot_register_t hear, cbot_register_t respond, cbot_send_t send_)
{
  send = send_;
  respond(bot, "([Yy]ou +[Ss]uck[!.]?|"
          "[Ss]ucks[!.]?|"
          "[Ii] +[Hh]ate +[Yy]ou[!.]?|"
          "[Ss]hut [Uu]p[!.]?)", sadness);
}
