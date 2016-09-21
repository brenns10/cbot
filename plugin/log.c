/***************************************************************************//**

  @file         log.c

  @author       Stephen Brennan

  @date         Created Wednesday, 21 September 2016

  @brief        Logging plugin for CBot.

  @copyright    Copyright (c) 2016, Stephen Brennan.  Released under the
                Revised BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#define _XOPEN_SOURCE 700

#include <string.h>
#include <time.h>
#include <stdio.h>

#include "cbot/cbot.h"
#include "libstephen/cb.h"

static void write_string(FILE *f, const char *str)
{
  size_t i;
  fputc('"', f);
  for (i = 0; str[i]; i++) {
    switch (str[i]) {
    case '\n':
      fprintf(f, "\\n");
      break;
    case '"':
      fprintf(f, "\"");
      break;
    default:
      fputc(str[i], f);
    }
  }
  fputc('"', f);
}

/*
  For every channel message, get the current timestamp, open a file of the form:
      CHANNEL-YYYY-MM-DD.log
  In append mode, and write out a single-line JSON object, containing:
  - timestamp: seconds since the epoch, as a float
  - username: sender of the message
  - message: content of message
 */
static void cbot_log(cbot_event_t event, cbot_actions_t actions)
{
  #define NSEC_PER_SEC 10000000000.0
  struct timespec now;
  struct tm *tm;
  double time_float;
  cbuf filename;
  FILE *f;

  /*
    First get timestamp.
  */
  clock_gettime(CLOCK_REALTIME, &now);
  tm = localtime(&now.tv_sec);
  time_float = now.tv_sec + now.tv_nsec / NSEC_PER_SEC;

  /*
    Create filename and open it.
  */
  cb_init(&filename, 40);
  cb_printf(&filename, "%s-%04d-%04d-%02d.log", event.channel,
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
  f = fopen(filename.buf, "a");

  /*
    Write log line.
  */
  fprintf(f, "{\"timestamp\": %f, \"username\": ", time_float);
  write_string(f, event.username);
  fprintf(f, ", \"message\": ");
  write_string(f, event.message);
  fprintf(f, "}\n");

  /*
    Cleanup
  */
  fclose(f);
  cb_destroy(&filename);
}

void log_load(cbot_t *bot, cbot_register_t registrar)
{
  registrar(bot, CBOT_CHANNEL_MSG, cbot_log);
}
