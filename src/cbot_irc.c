/***************************************************************************//**

  @file         cbot_irc.c

  @author       Stephen Brennan

  @date         Created Thursday, 23 July 2015

  @brief        CBot implementation for IRC!

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <libircclient/libircclient.h>
#include <libircclient/libirc_rfcnumeric.h>
#include "libstephen/cb.h"

#include "cbot_handlers.h"
#include "cbot_private.h"
#include "cbot_irc.h"

#define IRC_HOST "irc.case.edu"
#define IRC_PORT 6667
#define IRC_PASS NULL
#define IRC_NICK "cbot"
#define IRC_USER NULL
#define IRC_NAME NULL
#define IRC_CHANNEL "#cwru"

void log_event(irc_session_t *session, const char *event,
               const char *origin, const char **params, unsigned int count)
{
  size_t i;
  printf("Event \"%s\", origin: \"%s\", params: %d [", event, origin, count);
  for (i = 0; i < count; i++) {
    if (i != 0) {
      fputc('|', stdout);
    }
    printf("%s", params[i]);
  }
  printf("]\n");
}

void event_numeric(irc_session_t *session, unsigned int event,
                   const char *origin, const char **params, unsigned int count)
{
  char buf[24];
  sprintf(buf, "%d", event);
  log_event(session, buf, origin, params, count);
}

void event_join(irc_session_t *session, const char *event,
                const char *origin, const char **params, unsigned int count)
{
  log_event(session, event, origin, params, count);
}

/*
  Run once we are connected to the server.
 */
void event_connect(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
  irc_cmd_join(session, IRC_CHANNEL, NULL);
  log_event(session, event, origin, params, count);
}

void cbot_irc_send(cbot_t *cbot, const char *dest, char *format, va_list args)
{
  cbuf cb;
  irc_session_t *session = cbot->backend;
  cb_init(&cb, 1024);
  cb_vprintf(&cb, format, args);
  irc_cmd_msg(session, dest, cb.buf);
  cb_destroy(&cb);
}

void event_channel(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
  log_event(session, event, origin, params, count);
  if (count >= 2 && params[1] != NULL) {
    printf("sending to cbot\n");
    cbot_handle_message(irc_get_ctx(session), params[0], origin, params[1]);
    printf("handled by cbot\n");
  }
  printf("event done!\n");
}

void run_cbot_irc(void)
{
  irc_callbacks_t callbacks;
  irc_session_t *session;
  cbot_t *cbot = cbot_create(IRC_NICK, cbot_irc_send);

  memset(&callbacks, 0, sizeof(callbacks));

  callbacks.event_connect = event_connect;
  callbacks.event_join = event_join;
  callbacks.event_nick = log_event;
  callbacks.event_quit = log_event;
  callbacks.event_part = log_event;
  callbacks.event_mode = log_event;
  callbacks.event_topic = log_event;
  callbacks.event_kick = log_event;
  callbacks.event_channel = event_channel;
  callbacks.event_privmsg = log_event;
  callbacks.event_notice = log_event;
  callbacks.event_invite = log_event;
  callbacks.event_umode = log_event;
  callbacks.event_ctcp_rep = log_event;
  callbacks.event_ctcp_action = log_event;
  callbacks.event_unknown = log_event;
  callbacks.event_numeric = event_numeric;

  session = irc_create_session(&callbacks);
  if (!session) {
    fprintf(stderr, "cbot: error creating IRC session - %s\n",
            irc_strerror(irc_errno(session)));
    exit(EXIT_FAILURE);
  }

  cbot->backend = session;
  cbot_handlers_register(cbot);

  // Set libircclient to parse nicknames for us.
  irc_option_set(session, LIBIRC_OPTION_STRIPNICKS);
  // Set libircclient to ignore invalid certificates (irc.case.edu...)
  irc_option_set(session, LIBIRC_OPTION_SSL_NO_VERIFY);

  // Save the cbot struct in the irc context
  irc_set_ctx(session, cbot);

  // Start the connection process!
  if (irc_connect(session, IRC_HOST, IRC_PORT, IRC_PASS, IRC_NICK, IRC_USER, IRC_NAME)) {
    fprintf(stderr, "cbot: error connecting to IRC - %s\n",
            irc_strerror(irc_errno(session)));
    exit(EXIT_FAILURE);
  }

  // Run the networking event loop.
  if (irc_run(session)) {
    fprintf(stderr, "cbot: error running IRC connection loop - %s\n",
            irc_strerror(irc_errno(session)));
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
