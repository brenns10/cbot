/***************************************************************************//**

  @file         cbot.h

  @author       Stephen Brennan

  @date         Created Wednesday, 22 July 2015

  @brief        Declarations for CBot!

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#ifndef CBOT_H
#define CBOT_H

struct cbot;
typedef struct cbot cbot_t;

typedef void (*cbot_callback_t)(cbot_t *bot, const char *channel, const char* user, const char* message);

void cbot_register_hear(cbot_t *bot, const char *regex, cbot_callback_t callback);
void cbot_register_respond(cbot_t *bot, const char *regex, cbot_callback_t callback);

void cbot_send(cbot_t *bot, const char *dest, char *format, ...);

#endif//CBOT_H
