/**
 * sqlkarma.c: CBot plugin which can track karma in SQL
 */
#include <libconfig.h>
#include <stdlib.h>
#include <string.h>

#include "cbot/cbot.h"
#include "cbot/db.h"
#include "sc-collections.h"
#include "sc-regex.h"

struct cbot;

const char *tbl_karma_alters[] = {};

const struct cbot_db_table tbl_karma = {
	.name = "karma",
	.version = 0,
	.create = "CREATE TABLE karma ( "
	          "  item TEXT NOT NULL UNIQUE, "
	          "  karma INT NOT NULL "
	          ");",
	.alters = tbl_karma_alters,
};

struct karma {
	char *word;
	int karma;
	struct sc_list_head list;
};

#define KARMA_TOP 5

static int karma_query_get(struct cbot *bot, char *word, int *karma)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "SELECT karma FROM karma WHERE item=$word;");
	CBOTDB_BIND_ARG(text, word);
	CBOTDB_SINGLE_INTPTR_RESULT(karma);
}

static int karma_query_del(struct cbot *bot, char *word)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "DELETE FROM karma WHERE item=$word;");
	CBOTDB_BIND_ARG(text, word);
	CBOTDB_NO_RESULT();
}

static int karma_query_update_by(struct cbot *bot, char *word, int adjust)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "INSERT INTO karma(item, karma) "
	                        "VALUES($word, $adjust) "
	                        "ON CONFLICT(item) DO UPDATE "
	                        "SET karma=karma + excluded.karma;");
	CBOTDB_BIND_ARG(text, word);
	CBOTDB_BIND_ARG(int, adjust);
	CBOTDB_NO_RESULT();
}

static int karma_query_set(struct cbot *bot, char *word, int value)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "INSERT INTO karma(item, karma) "
	                        "VALUES($word, $value) "
	                        "ON CONFLICT(item) DO UPDATE "
	                        "SET karma=excluded.karma;");
	CBOTDB_BIND_ARG(text, word);
	CBOTDB_BIND_ARG(int, value);
	CBOTDB_NO_RESULT();
}

static int karma_query_top(struct cbot *bot, int limit,
                           struct sc_list_head *res)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, struct karma,
	                        "SELECT item, karma FROM karma "
	                        "ORDER BY karma DESC LIMIT $limit;");
	CBOTDB_BIND_ARG(int, limit);
	CBOTDB_LIST_RESULT(bot, res,
	                   /* noformat */
	                   CBOTDB_OUTPUT(text, 0, word);
	                   CBOTDB_OUTPUT(int, 1, karma););
}

static void karma_best(struct cbot_message_event *event)
{
	struct sc_list_head res;
	struct karma *k, *n;

	sc_list_init(&res);
	karma_query_top(event->bot, KARMA_TOP, &res);
	sc_list_for_each_safe(k, n, &res, list, struct karma)
	{
		cbot_send(event->bot, event->channel, "%s: %d", k->word,
		          k->karma);
		free(k->word);
		free(k);
	}
}

static void karma_check(struct cbot_message_event *event, void *user)
{
	int rv, karma = 0;
	char *word = sc_regex_get_capture(event->message, event->indices, 1);

	// An empty capture means we should list out the best karma.
	if (strcmp(word, "") == 0) {
		free(word);
		karma_best(event);
		return;
	}

	rv = karma_query_get(event->bot, word, &karma);
	if (rv < 0) {
		cbot_send(event->bot, event->channel, "%s has no karma yet",
		          word);
	} else {
		cbot_send(event->bot, event->channel, "%s has %d karma", word,
		          karma);
	}
	free(word);
}

static void karma_change(struct cbot_message_event *event, void *user)
{
	char *word = sc_regex_get_capture(event->message, event->indices, 0);
	char *op = sc_regex_get_capture(event->message, event->indices, 1);
	int adj = (strcmp(op, "++") == 0 ? 1 : -1);
	karma_query_update_by(event->bot, word, adj);
	free(word);
	free(op);
}

static void karma_set(struct cbot_message_event *event, void *user)
{
	char *word, *value;
	if (!cbot_is_authorized(event->bot, event->username, event->message)) {
		cbot_send(event->bot, event->channel,
		          "sorry, you're not authorized to do that!");
		return;
	}

	word = sc_regex_get_capture(event->message, event->indices, 0);
	value = sc_regex_get_capture(event->message, event->indices, 1);
	karma_query_set(event->bot, word, atoi(value));
	free(word);
	free(value);
}

static void karma_forget(struct cbot_message_event *event, void *user)
{
	karma_query_del(event->bot, (char *)event->username);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	int rv;

	(void)conf;

	rv = cbot_db_register(plugin, &tbl_karma);
	if (rv < 0)
		return rv;

#define KARMA_WORD     "^ \t\n"
#define NOT_KARMA_WORD " \t\n"
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)karma_check, NULL,
	              "karma(\\s+([" KARMA_WORD "]+))?");
	cbot_register(plugin, CBOT_MESSAGE, (cbot_handler_t)karma_change, NULL,
	              ".*?([" KARMA_WORD "]+)(\\+\\+|--).*?");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)karma_set, NULL,
	              "set-karma +([" KARMA_WORD "]+) +(-?\\d+) *.*");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)karma_forget,
	              NULL, "forget[ -]me");

	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
