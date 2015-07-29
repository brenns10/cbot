/***************************************************************************//**

  @file         help.c

  @author       Stephen Brennan

  @date         Created Wednesday, 29 July 2015

  @brief        CBot plugin to give help.

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <stdlib.h>
#include "cbot/cbot.h"

static cbot_send_t send;

static char *help_lines[] = {
#include "help.h"
};

static void help(cbot_t *bot, const char *channel, const char *user,
                 const char *message)
{
  int i;
  for (i = 0; i < sizeof(help_lines)/sizeof(char*); i++) {
    send(bot, user, help_lines[i]);
  }
}

void help_load(cbot_t *bot, cbot_register_t hear, cbot_register_t respond,
               cbot_send_t send_)
{
  send = send_;
  respond(bot, "[Hh][Ee][Ll][Pp].*", help);
}
