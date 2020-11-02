/**
 * cbot_irc.c: IRC backend for CBot
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sc-argparse.h>
#include <sc-collections.h>
#include <sc-lwt.h>

#include "libirc_rfcnumeric.h"
#include "libircclient.h"

#include "cbot_handlers.h"
#include "cbot_irc.h"
#include "cbot_private.h"

char *chan = "#cbot";
char *chanpass = NULL;

static inline struct cbot *session_bot(irc_session_t *session)
{
	return irc_get_ctx(session);
}

static inline struct cbot_irc_backend *session_irc(irc_session_t *session)
{
	return session_bot(session)->backend->backend_data;
}

static inline struct cbot_irc_backend *bot_irc(const struct cbot *bot)
{
	return bot->backend->backend_data;
}

static inline irc_session_t *bot_session(const struct cbot *bot)
{
	return bot_irc(bot)->session;
}

static struct join_rq *join_rq_new(struct cbot_irc_backend *irc,
                                   const char *chan)
{
	struct join_rq *rq;
	rq = calloc(1, sizeof(*rq));
	rq->channel = strdup(chan);
	sc_list_insert_end(&irc->join_rqs, &rq->list);
	return rq;
}

static void join_rq_delete(struct cbot_irc_backend *irc, struct join_rq *rq)
{
	sc_list_remove(&rq->list);
	free(rq->channel);
	free(rq);
}

static struct names_rq *names_rq_new(struct cbot_irc_backend *irc,
                                     const char *chan)
{
	struct names_rq *rq;
	rq = calloc(1, sizeof(*rq));
	rq->channel = strdup(chan);
	sc_list_insert_end(&irc->names_rqs, &rq->list);
	sc_cb_init(&rq->names, 4096);
	return rq;
}

static void names_rq_delete(struct cbot_irc_backend *irc, struct names_rq *rq)
{
	sc_list_remove(&rq->list);
	free(rq->channel);
	sc_cb_destroy(&rq->names);
	free(rq);
}

static struct topic_rq *topic_rq_new(struct cbot_irc_backend *irc,
                                     const char *chan)
{
	struct topic_rq *rq;
	rq = calloc(1, sizeof(*rq));
	rq->channel = strdup(chan);
	sc_list_insert_end(&irc->topic_rqs, &rq->list);
	return rq;
}

static void topic_rq_delete(struct cbot_irc_backend *irc, struct topic_rq *rq)
{
	sc_list_remove(&rq->list);
	free(rq->channel);
	if (rq->topic)
		free(rq->topic);
	free(rq);
}

void *lookup_by_str(struct sc_list_head *list, const char *str)
{
	struct sc_list_head *node;
	char **after;

	sc_list_for_each(node, list)
	{
		after = (char **)&node[1];
		if (strcmp(*after, str) == 0)
			return node;
	}

	return NULL;
}

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

static void add_all_names(struct cbot *bot, struct names_rq *rq)
{
	char *nick = strtok(rq->names.buf, " ");
	do {
		if (nick[0] == '~' || nick[0] == '&' || nick[0] == '+' ||
		    nick[0] == '%') {
			nick++;
		}
		/* TODO bulk db insertion API? */
		cbot_add_membership(bot, nick, rq->channel);
	} while ((nick = strtok(NULL, " ")) != NULL);
}

static void join_maybe_done(struct cbot_irc_backend *irc, struct join_rq *rq)
{
	if (rq->received_names && rq->received_topics) {
		add_all_names(irc->bot, rq->names_rq);
		if (rq->topic_rq->topic)
			cbot_set_channel_topic(irc->bot, rq->channel,
			                       rq->topic_rq->topic);
		names_rq_delete(irc, rq->names_rq);
		topic_rq_delete(irc, rq->topic_rq);
		join_rq_delete(irc, rq);
	}
}

static void event_rpl_namreply(irc_session_t *session, const char *origin,
                               const char **params, unsigned int count)
{
	struct cbot_irc_backend *irc = session_irc(session);
	struct names_rq *rq = lookup_by_str(&irc->names_rqs, params[2]);
	if (!rq) {
		fprintf(stderr, "ERR: unsolicited RPL_NAMREPLY for %s\n",
		        params[2]);
		return;
	}
	sc_cb_concat(&rq->names, (char *)params[3]);
}

void event_rpl_endofnames(irc_session_t *session, const char *origin,
                          const char **params, unsigned int count)
{
	struct cbot_irc_backend *irc = session_irc(session);
	struct cbot *bot = session_bot(session);
	struct names_rq *rq = lookup_by_str(&irc->names_rqs, params[1]);
	if (!rq) {
		fprintf(stderr, "ERR: unsolicited RPL_ENDOFNAMES for %s\n",
		        params[1]);
		return;
	}
	if (rq->join_rq) {
		rq->join_rq->received_names = true;
		join_maybe_done(irc, rq->join_rq);
	} else {
		/*
		 * This is in response to an explicit join request sent by us.
		 * For some reason. So clear the existing memberships and
		 * refresh from this response.
		 */
		cbot_clear_channel_memberships(bot, rq->channel);
		add_all_names(bot, rq);
		names_rq_delete(irc, rq);
	}
}

void event_rpl_topic(irc_session_t *session, const char *origin,
                     const char **params, unsigned int count)
{
	/* TODO: notopic? */
	struct cbot_irc_backend *irc = session_irc(session);
	struct cbot *bot = session_bot(session);
	struct topic_rq *rq = lookup_by_str(&irc->topic_rqs, params[1]);
	if (!rq) {
		fprintf(stderr, "ERR: unsolicited RPL_TOPIC for %s\n",
		        params[1]);
		return;
	}
	if (params[2])
		rq->topic = strdup(params[2]);
	if (rq->join_rq) {
		rq->join_rq->received_topics = true;
		join_maybe_done(irc, rq->join_rq);
	} else {
		cbot_set_channel_topic(bot, rq->channel, rq->topic);
		topic_rq_delete(irc, rq);
	}
}

void event_numeric(irc_session_t *session, unsigned int event,
                   const char *origin, const char **params, unsigned int count)
{
	char buf[24];
	sprintf(buf, "%d", event);
	switch (event) {
	case 332:
		event_rpl_topic(session, origin, params, count);
		break;
	case 353:
		event_rpl_namreply(session, origin, params, count);
		break;
	case 366:
		event_rpl_endofnames(session, origin, params, count);
		break;
	}
	log_event(session, buf, origin, params, count);
}

/*
  Run once we are connected to the server.
 */
void event_connect(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
	cbot_join(session_bot(session), chan, chanpass);
	irc_cmd_join(session, chan, chanpass);
	log_event(session, event, origin, params, count);
}

static inline void maybe_schedule(const struct cbot *bot)
{
	if (sc_lwt_current() != bot->lwt)
		sc_lwt_set_state(bot->lwt, SC_LWT_RUNNABLE);
}

static void cbot_irc_send(const struct cbot *cbot, const char *to,
                          const char *msg)
{
	irc_session_t *session = bot_session(cbot);
	irc_cmd_msg(session, to, msg);
	maybe_schedule(cbot);
}

static void cbot_irc_me(const struct cbot *cbot, const char *to,
                        const char *msg)
{
	irc_session_t *session = bot_session(cbot);
	irc_cmd_me(session, to, msg);
	maybe_schedule(cbot);
}

static void cbot_irc_op(const struct cbot *cbot, const char *channel,
                        const char *username)
{
	irc_session_t *session = bot_session(cbot);
	struct sc_charbuf cb;
	sc_cb_init(&cb, 256);
	sc_cb_printf(&cb, "+o %s", username);
	irc_cmd_channel_mode(session, channel, cb.buf);
	sc_cb_destroy(&cb);
}

static void cbot_irc_nick(const struct cbot *cbot, const char *newnick)
{
	irc_session_t *session = bot_session(cbot);
	irc_cmd_nick(session, newnick);
	maybe_schedule(cbot);
}

static void cbot_irc_join(const struct cbot *cbot, const char *channel,
                          const char *password)
{
	irc_session_t *session = bot_session(cbot);
	struct cbot_irc_backend *irc = bot_irc(cbot);
	struct join_rq *join_rq;

	/* Joining triggers a request for join, as well as names and topic */
	join_rq = join_rq_new(irc, channel);
	join_rq->names_rq = names_rq_new(irc, channel);
	join_rq->names_rq->join_rq = join_rq;
	join_rq->topic_rq = topic_rq_new(irc, channel);
	join_rq->topic_rq->join_rq = join_rq;

	irc_cmd_join(session, channel, password);
	maybe_schedule(cbot);
}

void event_channel(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	if (count >= 2 && params[1] != NULL) {
		cbot_handle_message(session_bot(session), params[0], origin,
		                    params[1], false);
		printf("Event handled by CBot.\n");
	}
}

void event_action(irc_session_t *session, const char *event, const char *origin,
                  const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = session_bot(session);
	cbot_handle_message(bot, params[0], origin, params[1], true);
	printf("Event handled by CBot.\n");
}

void event_join(irc_session_t *session, const char *event, const char *origin,
                const char **params, unsigned int count)
{
	struct join_rq *rq;
	log_event(session, event, origin, params, count);
	struct cbot_irc_backend *irc = session_irc(session);
	struct cbot *bot = irc->bot;
	if (strcmp(origin, bot->name) == 0) {
		rq = lookup_by_str(&irc->join_rqs, params[0]);
		if (!rq) {
			fprintf(stderr, "ERR: unsolicited self JOIN %s\n",
			        params[0]);
		} else {
			rq->received_join = true;
			join_maybe_done(irc, rq);
		}
	} else {
		cbot_handle_user_event(bot, params[0], origin, CBOT_JOIN);
	}
	printf("Event handled by CBot.\n");
}

void event_part(irc_session_t *session, const char *event, const char *origin,
                const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = session_bot(session);
	cbot_handle_user_event(bot, params[0], origin, CBOT_PART);
	printf("Event handled by CBot.\n");
}

void event_nick(irc_session_t *session, const char *event, const char *origin,
                const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = session_bot(session);
	if (strcmp(origin, bot->name) == 0)
		cbot_set_nick(bot, params[0]);
	else
		cbot_handle_nick_event(bot, origin, params[0]);
	printf("Event handled by CBot.\n");
}

static void cbot_irc_lwt(void *data)
{
	struct cbot *bot = data;
	irc_session_t *session = bot_session(bot);
	fd_set in_fd, out_fd;
	int maxfd;
	struct sc_lwt_poll poll[16];
	struct sc_lwt *cur = sc_lwt_current();
	int count;

	while (1) {
		FD_ZERO(&in_fd);
		FD_ZERO(&out_fd);
		maxfd = 0;
		irc_add_select_descriptors(session, &in_fd, &out_fd, &maxfd);
		for (int i = 0; i <= maxfd; i++) {
			int flags = 0;
			if (FD_ISSET(i, &in_fd))
				flags |= SC_LWT_W_IN;
			if (FD_ISSET(i, &out_fd))
				flags |= SC_LWT_W_OUT;
			if (flags) {
				sc_lwt_wait_fd(cur, i, flags, NULL);
			}
		}
		sc_lwt_set_state(cur, SC_LWT_BLOCKED);
		sc_lwt_yield();
		count = sc_lwt_ready_fds(cur, poll, nelem(poll));
		if (count < 0) {
			fprintf(stderr, "err: sc_lwt_ready_fds returned %d\n",
			        count);
			exit(EXIT_FAILURE);
		} else if (count == nelem(poll)) {
			fprintf(stderr, "warn: may have missed an fd\n");
		}
		FD_ZERO(&in_fd);
		FD_ZERO(&out_fd);
		for (int i = 0; i < count; i++) {
			if (poll[i].event & SC_LWT_W_IN) {
				FD_SET(poll[i].fd, &in_fd);
			} else if (poll[i].event & SC_LWT_W_OUT) {
				FD_SET(poll[i].fd, &out_fd);
			}
		}
		irc_process_select_descriptors(session, &in_fd, &out_fd);
		sc_lwt_remove_all(cur);
	}
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
	/* noformat */
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
	struct cbot_irc_backend irc_backend;
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

		SC_ARG_END()
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

	sc_list_init(&irc_backend.join_rqs);
	sc_list_init(&irc_backend.topic_rqs);
	sc_list_init(&irc_backend.names_rqs);

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
	irc_backend.bot = cbot;
	irc_backend.session = session;
	backend.backend_data = &irc_backend;

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
	cbot->lwt_ctx = sc_lwt_init();
	cbot->lwt = sc_lwt_create_task(cbot->lwt_ctx, cbot_irc_lwt, cbot);
	sc_lwt_run(cbot->lwt_ctx);
	exit(EXIT_SUCCESS);
}
