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

struct names_rq {
	struct sc_list_head list;
	char *channel;
	struct sc_charbuf names;
};

struct cbot_irc_backend {
	irc_session_t *session;
	irc_callbacks_t callbacks;
	struct cbot *bot;
	bool connected;
	struct sc_list_head join_rqs;
	struct sc_list_head topic_rqs;
	struct sc_list_head names_rqs;
	char *host;
	int port;
	char *password;
};

#endif // CBOT_IRC_H
