/**
 * cbot_irc.c: IRC backend for CBot
 */

#include <libconfig.h>
#include <sc-collections.h>
#include <sc-lwt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "cbot/cbot.h"
#include "cbot_irc.h"
#include "cbot_private.h"
#include "libircclient.h"

static inline struct cbot *session_bot(irc_session_t *session)
{
	return irc_get_ctx(session);
}

static inline struct cbot_irc_backend *session_irc(irc_session_t *session)
{
	return session_bot(session)->backend;
}

static inline struct cbot_irc_backend *bot_irc(const struct cbot *bot)
{
	return bot->backend;
}

static inline irc_session_t *bot_session(const struct cbot *bot)
{
	return bot_irc(bot)->session;
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
	cbot_clear_channel_memberships(bot, rq->channel);
	add_all_names(bot, rq);
	names_rq_delete(irc, rq);
}

void event_rpl_topic(irc_session_t *session, const char *origin,
                     const char **params, unsigned int count)
{
	struct cbot *bot = session_bot(session);
	cbot_set_channel_topic(bot, (char *)params[1], (char *)params[2]);
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
	struct cbot *bot = session_bot(session);
	struct cbot_channel_conf *c;
	sc_list_for_each_entry(c, &bot->init_channels, list,
	                       struct cbot_channel_conf)
	{
		cbot_join(bot, c->name, c->pass);
		irc_cmd_join(session, c->name, c->pass);
	}
	log_event(session, event, origin, params, count);
}

static inline void maybe_schedule(const struct cbot *bot)
{
	if (sc_lwt_current() != bot->lwt)
		sc_lwt_set_state(bot->lwt, SC_LWT_RUNNABLE);
}

static uint64_t cbot_irc_send(const struct cbot *cbot, const char *to,
                              const struct cbot_reaction_ops *ops, void *arg,
                              const char *msg)
{
	irc_session_t *session = bot_session(cbot);
	irc_cmd_msg(session, to, msg);
	maybe_schedule(cbot);
	return 0;
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

	/* Joining triggers a request for names, which we need to be prepared
	 * to handle */
	names_rq_new(irc, channel);
	/* topic replies we gracefully handle, same with join replies */

	irc_cmd_join(session, channel, password);
	maybe_schedule(cbot);
}

void event_privmsg(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	if (count >= 2 && params[1] != NULL) {
		cbot_handle_message(session_bot(session), origin, origin,
		                    params[1], false, true);
		printf("Event handled by CBot.\n");
	}
}

void event_channel(irc_session_t *session, const char *event,
                   const char *origin, const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	if (count >= 2 && params[1] != NULL) {
		cbot_handle_message(session_bot(session), params[0], origin,
		                    params[1], false, false);
		printf("Event handled by CBot.\n");
	}
}

void event_action(irc_session_t *session, const char *event, const char *origin,
                  const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot *bot = session_bot(session);
	cbot_handle_message(bot, params[0], origin, params[1], true, false);
	printf("Event handled by CBot.\n");
}

void event_join(irc_session_t *session, const char *event, const char *origin,
                const char **params, unsigned int count)
{
	log_event(session, event, origin, params, count);
	struct cbot_irc_backend *irc = session_irc(session);
	struct cbot *bot = irc->bot;
	if (strcmp(origin, bot->name) != 0) {
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

static void cbot_irc_run(struct cbot *bot)
{
	irc_session_t *session = bot_session(bot);
	struct cbot_irc_backend *irc = bot_irc(bot);
	fd_set in_fd, out_fd, err_fd;
	int maxfd, rv;
	struct sc_lwt *cur = sc_lwt_current();

	// Start the connection process!
	if (irc_connect(session, irc->host, irc->port, irc->password, bot->name,
	                bot->name, NULL)) {
		fprintf(stderr, "cbot: error connecting to IRC - %s\n",
		        irc_strerror(irc_errno(session)));
		exit(EXIT_FAILURE);
	}

	while (1) {
		sc_lwt_fdgen_advance(cur);
		sc_lwt_clear_fds(&in_fd, &out_fd, &err_fd);
		maxfd = 0;
		rv = irc_add_select_descriptors(session, &in_fd, &out_fd,
		                                &maxfd);
		if (rv != 0) {
			fprintf(stderr, "cbot_irc: irc error: %s\n",
			        irc_strerror(irc_errno(session)));
			break;
		}
		sc_lwt_add_select_fds(cur, &in_fd, &out_fd, &err_fd, maxfd,
		                      NULL);
		sc_lwt_fdgen_purge(cur);

		sc_lwt_set_state(cur, SC_LWT_BLOCKED);
		sc_lwt_yield();
		sc_lwt_clear_fds(&in_fd, &out_fd, &err_fd);
		sc_lwt_populate_ready_fds(cur, &in_fd, &out_fd, &err_fd, maxfd);
		rv = irc_process_select_descriptors(session, &in_fd, &out_fd);
		if (rv != 0) {
			fprintf(stderr, "cbot_irc: irc error: %s\n",
			        irc_strerror(irc_errno(session)));
			break;
		}
	}
}

static int cbot_irc_configure(struct cbot *bot, config_setting_t *group)
{
	int rv;
	const char *host, *password;
	int port;
	struct cbot_irc_backend *backend;

	rv = config_setting_lookup_string(group, "host", &host);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr,
		        "cbot irc: key \"host\" wrong type or not exists\n");
		return -1;
	}
	rv = config_setting_lookup_int(group, "port", &port);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr,
		        "cbot irc: key \"port\" wrong type or not exists\n");
		return -1;
	}
	rv = config_setting_lookup_string(group, "password", &password);
	if (rv == CONFIG_FALSE)
		password = NULL;

	backend = calloc(1, sizeof(*backend));
	bot->backend = backend;
	backend->bot = bot;
	backend->host = strdup(host);
	backend->port = port;
	if (password)
		backend->password = strdup(password);

	sc_list_init(&backend->join_rqs);
	sc_list_init(&backend->topic_rqs);
	sc_list_init(&backend->names_rqs);

	backend->callbacks.event_connect = event_connect;
	backend->callbacks.event_join = event_join;
	backend->callbacks.event_nick = event_nick;
	backend->callbacks.event_quit = log_event;
	backend->callbacks.event_part = event_part;
	backend->callbacks.event_mode = log_event;
	backend->callbacks.event_topic = log_event;
	backend->callbacks.event_kick = log_event;
	backend->callbacks.event_channel = event_channel;
	backend->callbacks.event_privmsg = event_privmsg;
	backend->callbacks.event_notice = log_event;
	backend->callbacks.event_invite = log_event;
	backend->callbacks.event_umode = log_event;
	backend->callbacks.event_ctcp_rep = log_event;
	backend->callbacks.event_ctcp_action = log_event;
	backend->callbacks.event_unknown = log_event;
	backend->callbacks.event_numeric = event_numeric;

	backend->session = irc_create_session(&backend->callbacks);
	if (!backend->session) {
		fprintf(stderr, "cbot: error creating IRC session - %s\n",
		        irc_strerror(irc_errno(backend->session)));
		return -1;
	}
	// Set libircclient to parse nicknames for us.
	irc_option_set(backend->session, LIBIRC_OPTION_STRIPNICKS);
	// Set libircclient to ignore invalid certificates (irc.case.edu...)
	irc_option_set(backend->session, LIBIRC_OPTION_SSL_NO_VERIFY);
	// Save the cbot struct in the irc context
	irc_set_ctx(backend->session, bot);
	return 0;
}

struct cbot_backend_ops irc_ops = {
	.name = "irc",
	.configure = cbot_irc_configure,
	.run = cbot_irc_run,
	.send = cbot_irc_send,
	.me = cbot_irc_me,
	.op = cbot_irc_op,
	.join = cbot_irc_join,
	.nick = cbot_irc_nick,
	.is_authorized = NULL,
};
