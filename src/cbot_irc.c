/**
 * cbot_irc.c: IRC backend for CBot
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sc-argparse.h>
#include <sc-collections.h>

#include "libirc_rfcnumeric.h"
#include "libircclient.h"

#include "cbot_handlers.h"
#include "cbot_irc.h"
#include "cbot_private.h"

char *chan = "#cbot";
char *chanpass = NULL;

void log_event(irc_session_t *session, const char *event, const char *origin,
               const char **params, unsigned int count)
{
	size_t i;
	printf("Event \"%s\", origin: \"%s\", params: %d [", event, origin,
	       count);
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

/*
  Run once we are connected to the server.
 */
void event_connect(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
	irc_cmd_join(session, chan, chanpass);
	log_event(session, event, origin, params, count);
}

static void cbot_irc_send(const struct cbot *cbot, const char *to,
                          const char *msg)
{
	irc_session_t *session = cbot->backend->backend_data;
	irc_cmd_msg(session, to, msg);
}

static void cbot_irc_me(const struct cbot *cbot, const char *to,
                        const char *msg)
{
	irc_session_t *session = cbot->backend->backend_data;
	irc_cmd_me(session, to, msg);
}

static void cbot_irc_op(const struct cbot *cbot, const char *channel,
                        const char *username)
{
	irc_session_t *session = cbot->backend->backend_data;
	struct sc_charbuf cb;
	sc_cb_init(&cb, 256);
	sc_cb_printf(&cb, "+o %s", username);
	irc_cmd_channel_mode(session, channel, cb.buf);
	sc_cb_destroy(&cb);
}

static void cbot_irc_nick(const struct cbot *cbot, const char *newnick)
{
	irc_session_t *session = cbot->backend->backend_data;
	irc_cmd_nick(session, newnick);
}

static void cbot_irc_join(const struct cbot *cbot, const char *channel,
                          const char *password)
{
	irc_session_t *session = cbot->backend->backend_data;
	irc_cmd_join(session, channel, password);
}

void event_channel(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	if (count >= 2 && params[1] != NULL) {
		cbot_handle_message(irc_get_ctx(session), params[0], origin,
		                    params[1], false);
		printf("Event handled by CBot.\n");
	}
}

void event_action(irc_session_t *session, const char *event, const char *origin,
                  const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = irc_get_ctx(session);
	cbot_handle_message(bot, params[0], origin, params[1], true);
	printf("Event handled by CBot.\n");
}

void event_join(irc_session_t *session, const char *event, const char *origin,
                const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = irc_get_ctx(session);
	cbot_handle_user_event(bot, params[0], origin, CBOT_JOIN);
	printf("Event handled by CBot.\n");
}

void event_part(irc_session_t *session, const char *event, const char *origin,
                const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = irc_get_ctx(session);
	cbot_handle_user_event(bot, params[0], origin, CBOT_PART);
	printf("Event handled by CBot.\n");
}

void event_nick(irc_session_t *session, const char *event, const char *origin,
                const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = irc_get_ctx(session);
	if (strcmp(origin, bot->name) == 0)
		cbot_set_nick(bot, params[0]);
	else
		cbot_handle_nick_event(bot, origin, params[0]);
	printf("Event handled by CBot.\n");
}

void help()
{
	puts("usage: cbot irc [options] plugins");
	puts("required flags:");
	puts("  --hash HASH        set the hash chain tip (required)");
	puts("  --plugin-dir [dir] set the plugin directory");
	puts("  --name [name]      set the bot's name");
	puts("optional flags:");
	puts("  --host [host]      set the irc server hostname");
	puts("  --password [pass]  set the irc server password");
	puts("  --port [port]      set the irc port number");
	puts("  --chan [chan]      set the irc channel");
	puts("  --chanpass [pw]    set the channel password");
	puts("  --help             show this help and exit");
	puts("plugins:");
	puts("  list of names of plugins within plugin-dir (don't include "
	     "\".so\").");
	exit(EXIT_FAILURE);
}

enum {
	ARG_NAME = 0,
	ARG_PLUGIN_DIR,
	ARG_HASH,
	ARG_HOST,
	ARG_PASSWORD,
	ARG_PORT,
	ARG_CHAN,
	ARG_CHANPASS,
	ARG_HELP,
};

void run_cbot_irc(int argc, char *argv[])
{
	irc_callbacks_t callbacks;
	irc_session_t *session;
	struct cbot *cbot;
	struct cbot_backend backend;
	char *name, *host, *plugin_dir, *password, *hash;
	unsigned short port_num;
	int rv;
	struct sc_arg args[] = {
		SC_ARG_DEF_STRING('n', "--name", "cbot", "bot name"),
		SC_ARG_DEF_STRING('p', "--plugin-dir", "bin/release/plugin",
		                  "plugin directory"),
		SC_ARG_STRING('H', "--hash", "hash chain tip"),
		SC_ARG_DEF_STRING('t', "--host", "irc.case.edu", "server host"),
		SC_ARG_STRING('P', "--password", "server password"),
		SC_ARG_DEF_INT('p', "--port", 6667, "server port number"),
		SC_ARG_DEF_STRING('c', "--chan", "#cbot", "channel to join"),
		SC_ARG_STRING('w', "--chanpass", "channel password"),
		SC_ARG_COUNT('h', "--help", "show help and exit"),
	};

	if ((rv = sc_argparse(args, argc, argv)) < 0) {
		fprintf(stderr, "error parsing args\n");
		help();
	}

	if (args[ARG_HELP].val_int)
		help();

	name = args[ARG_NAME].val_string;
	host = args[ARG_HOST].val_string;
	port_num = args[ARG_PORT].val_int;
	plugin_dir = args[ARG_PLUGIN_DIR].val_string;
	password = args[ARG_PASSWORD].val_string;
	chan = args[ARG_CHAN].val_string;
	chanpass = args[ARG_CHANPASS].val_string;
	hash = args[ARG_HASH].val_string;

	backend.send = cbot_irc_send;
	backend.me = cbot_irc_me;
	backend.op = cbot_irc_op;
	backend.join = cbot_irc_join;
	backend.nick = cbot_irc_nick;

	cbot = cbot_create(name, &backend);

	// Set the hash in the bot.
	void *decoded = base64_decode(hash, 20);
	memcpy(cbot->hash, decoded, 20);
	free(decoded);

	memset(&callbacks, 0, sizeof(callbacks));

	callbacks.event_connect = event_connect;
	callbacks.event_join = event_join;
	callbacks.event_nick = event_nick;
	callbacks.event_quit = log_event;
	callbacks.event_part = event_part;
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
	backend.backend_data = session;

	cbot->backend = &backend;
	cbot_load_plugins(cbot, plugin_dir, argv, rv);

	// Set libircclient to parse nicknames for us.
	irc_option_set(session, LIBIRC_OPTION_STRIPNICKS);
	// Set libircclient to ignore invalid certificates (irc.case.edu...)
	irc_option_set(session, LIBIRC_OPTION_SSL_NO_VERIFY);

	// Save the cbot struct in the irc context
	irc_set_ctx(session, cbot);

	// Start the connection process!
	if (irc_connect(session, host, port_num, password, name, name, NULL)) {
		fprintf(stderr, "cbot: error connecting to IRC - %s\n",
		        irc_strerror(irc_errno(session)));
		exit(EXIT_FAILURE);
	}

	// Run the networking event loop.
	if (irc_run(session)) {
		fprintf(stderr,
		        "cbot: error running IRC connection loop - %s\n",
		        irc_strerror(irc_errno(session)));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
