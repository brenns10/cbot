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


static char *help_lines[] = {
#include "help.h"
};

static void help(cbot_event_t event, cbot_actions_t actions)
{
  int i;
  for (i = 0; i < sizeof(help_lines)/sizeof(char*); i++) {
    actions.send(event.bot, event.username, help_lines[i]);
  }
}

void help_load(cbot_t *bot, cbot_register_t registrar)
{
  registrar(bot, CBOT_CHANNEL_MSG, "[Hh][Ee][Ll][Pp].*", help);
}
