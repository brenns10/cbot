/***************************************************************************//**

  @file         name.c

  @author       Stephen Brennan

  @date         Created Thursday, 23 July 2015

  @brief        CBot plugin to respond to questions about cbot.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include "cbot/cbot.h"

static cbot_send_t send;

static void name(cbot_t *bot, const char *channel, const char *user,
                 const char *message, const size_t *starts, const size_t *ends,
                 size_t ncaptures)
{
  send(bot, channel, "My name is CBot.  My source lives at https://github.com/brenns10/cbot");
}

void name_load(cbot_t *bot, cbot_register_t hear, cbot_register_t respond, cbot_send_t send_)
{
  send = send_;
  hear(bot, "([wW]ho|[wW]hat|[wW][tT][fF]) +([iI]s|[aA]re +[yY]ou,?) +[cC][bB]ot\\??", name);
}
