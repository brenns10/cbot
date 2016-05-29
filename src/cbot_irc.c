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
#include "libstephen/ll.h"
#include "libstephen/ad.h"
#include "libstephen/cb.h"

#include "cbot_handlers.h"
#include "cbot_private.h"
#include "cbot_irc.h"

char *chan = "#cbot";

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
  irc_cmd_join(session, chan, NULL);
  log_event(session, event, origin, params, count);
}

void cbot_irc_send(const cbot_t *cbot, const char *dest, const char *format, ...)
{
  va_list va;
  cbuf cb;
  irc_session_t *session = cbot->backend;
  va_start(va, format);
  cb_init(&cb, 1024);
  cb_vprintf(&cb, format, va);
  irc_cmd_msg(session, dest, cb.buf);
  cb_destroy(&cb);
  va_end(va);
}

void cbot_irc_me(const cbot_t *cbot, const char *dest, const char *format, ...)
{
  va_list va;
  cbuf cb;
  irc_session_t *session = cbot->backend;
  va_start(va, format);
  cb_init(&cb, 1024);
  cb_vprintf(&cb, format, va);
  irc_cmd_me(session, dest, cb.buf);
  cb_destroy(&cb);
  va_end(va);
}

void event_channel(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
  log_event(session, event, origin, params, count);
  if (count >= 2 && params[1] != NULL) {
    printf("sending to cbot\n");
    cbot_handle_channel_message(irc_get_ctx(session), params[0], origin, params[1]);
    printf("handled by cbot\n");
  }
  printf("event done!\n");
}

void help()
{
  puts("usage: cbot irc [options] plugins");
  puts("options:");
  puts("  --name [name]      set the bot's name");
  puts("  --host [host]      set the irc server hostname");
  puts("  --port [port]      set the irc port number");
  puts("  --chan [chan]      set the irc channel");
  puts("  --plugin-dir [dir] set the plugin directory");
  puts("  --help             show this help and exit");
  puts("plugins:");
  puts("  list of names of plugins within plugin-dir (don't include \".so\").");
  exit(EXIT_FAILURE);
}

void run_cbot_irc(int argc, char *argv[])
{
  irc_callbacks_t callbacks;
  irc_session_t *session;
  cbot_t *cbot;
  smb_ad args;
  char *name = "cbot";
  char *host = "irc.case.edu";
  char *port = "6667";
  char *plugin_dir = "plugin";
  unsigned short port_num;
  arg_data_init(&args);

  process_args(&args, argc, argv);

  if (check_long_flag(&args, "name")) {
    name = get_long_flag_parameter(&args, "name");
  }
  if (check_long_flag(&args, "host")) {
    host = get_long_flag_parameter(&args, "host");
  }
  if (check_long_flag(&args, "port")) {
    port = get_long_flag_parameter(&args, "port");
  }
  if (check_long_flag(&args, "chan")) {
    chan = get_long_flag_parameter(&args, "chan");
  }
  if (check_long_flag(&args, "plugin-dir")) {
    plugin_dir = get_long_flag_parameter(&args, "plugin-dir");
  }
  if (check_long_flag(&args, "help")) {
    help();
  }
  if (!(name && host && port && chan && plugin_dir)) {
    help();
  }
  sscanf(port, "%hu", &port_num);

  cbot = cbot_create(name);
  cbot->actions.send = cbot_irc_send;
  cbot->actions.me = cbot_irc_me;

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
  cbot_load_plugins(cbot, "plugin", ll_get_iter(args.bare_strings));

  // Set libircclient to parse nicknames for us.
  irc_option_set(session, LIBIRC_OPTION_STRIPNICKS);
  // Set libircclient to ignore invalid certificates (irc.case.edu...)
  irc_option_set(session, LIBIRC_OPTION_SSL_NO_VERIFY);

  // Save the cbot struct in the irc context
  irc_set_ctx(session, cbot);

  // Start the connection process!
  if (irc_connect(session, host, port_num, NULL, name, NULL, NULL)) {
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

  arg_data_destroy(&args);
  exit(EXIT_SUCCESS);
}
