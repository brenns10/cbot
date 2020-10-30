/**
 * cbot_irc.h
 */

#ifndef CBOT_IRC_H
#define CBOT_IRC_H

#include <stdbool.h>

#include "libirc_rfcnumeric.h"
#include "libircclient.h"

#include <sc-collections.h>

#include "cbot/cbot.h"

struct names_rq;
struct topic_rq;
struct join_rq {
	struct sc_list_head list;
	char *channel;
	struct names_rq *names_rq;
	struct topic_rq *topic_rq;
	bool received_names;
	bool received_topics;
	bool received_join;
};

struct names_rq {
	struct sc_list_head list;
	char *channel;
	struct join_rq *join_rq;
	struct sc_charbuf names;
};

struct topic_rq {
	struct sc_list_head list;
	char *channel;
	struct join_rq *join_rq;
	char *topic;
};

struct cbot_irc_backend {
	irc_session_t *session;
	struct cbot *bot;
	bool connected;
	struct sc_list_head join_rqs;
	struct sc_list_head topic_rqs;
	struct sc_list_head names_rqs;
};

#endif // CBOT_IRC_H
