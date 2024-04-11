/**
 * sqlknow.c: CBot plugin which remembers things
 */
#include <libconfig.h>
#include <stdlib.h>

#include "cbot/cbot.h"
#include "cbot/db.h"
#include "sc-collections.h"
#include "sc-regex.h"

struct cbot;

const char *tbl_knowledge_alters[] = {};

const struct cbot_db_table tbl_knowledge = {
	.name = "knowledge",
	.version = 0,
	.create = "CREATE TABLE knowledge ( "
	          "  key TEXT NOT NULL UNIQUE, "
	          "  value TEXT NOT NULL, "
	          "  nick TEXT NOT NULL, "
	          "  change_count INT NOT NULL "
	          ");",
	.alters = tbl_knowledge_alters,
};

struct knowledge {
	char *key;
	char *value;
	char *nick;
	int change_count;
};

static void knowledge_free(struct knowledge *k)
{
	free(k->key);
	free(k->value);
	free(k->nick);
	free(k);
}

static struct knowledge *knowledge_query_get(struct cbot *bot, char *key)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, struct knowledge,
	                        "SELECT key, value, nick, change_count "
	                        "FROM knowledge "
	                        "WHERE key=$key;");
	CBOTDB_BIND_ARG(text, key);
	CBOTDB_SINGLE_STRUCT_RESULT(
	        /* noformat */
	        CBOTDB_OUTPUT(text, 0, key); CBOTDB_OUTPUT(text, 1, value);
	        CBOTDB_OUTPUT(text, 2, nick);
	        CBOTDB_OUTPUT(int, 3, change_count););
}

static int knowledge_query_update(struct cbot *bot, char *key, char *value,
                                  char *nick)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, struct knowledge,
	                        "INSERT INTO knowledge("
	                        "    key, value, nick, change_count) "
	                        "VALUES($key, $value, $nick, 1) "
	                        "ON CONFLICT(key) DO UPDATE "
	                        "SET value=excluded.value, "
	                        "    nick=excluded.nick, "
	                        "    change_count=change_count + 1;");
	CBOTDB_BIND_ARG(text, key);
	CBOTDB_BIND_ARG(text, value);
	CBOTDB_BIND_ARG(text, nick);
	CBOTDB_NO_RESULT();
}

#define GET_VALUE ((void *)1)
#define GET_WHO   ((void *)2)

static void knowledge_get(struct cbot_message_event *event, void *user)
{
	char *key = sc_regex_get_capture(event->message, event->indices, 0);
	struct knowledge *k = knowledge_query_get(event->bot, key);
	if (!k) {
		cbot_send(event->bot, event->channel,
		          "Sorry, I don't know anything about %s", key);
	} else if (user == GET_VALUE) {
		cbot_send(event->bot, event->channel, "%s is %s", key,
		          k->value);
	} else {
		cbot_send(event->bot, event->channel,
		          "%s last taught me that %s is %s, but I have been "
		          "taught about it %d times",
		          k->nick, k->key, k->value, k->change_count);
	}
	if (k)
		knowledge_free(k);
	free(key);
}

static void knowledge_set(struct cbot_message_event *event, void *user)
{
	char *key = sc_regex_get_capture(event->message, event->indices, 0);
	char *value = sc_regex_get_capture(event->message, event->indices, 1);
	knowledge_query_update(event->bot, key, value, (char *)event->username);
	cbot_send(event->bot, event->channel, "ok, %s is %s", key, value);
	free(key);
	free(value);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	int rv;

	(void)conf;

	rv = cbot_db_register(plugin, &tbl_knowledge);
	if (rv < 0)
		return rv;

	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)knowledge_set,
	              NULL, "know that +(.+) +is +(.+)");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)knowledge_get,
	              GET_VALUE, "what is +(.+) *");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)knowledge_get,
	              GET_WHO, "who taught you about +(.+)\\??");

	return 0;
}

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(
	        cb,
	        "- cbot know that SOMETHING is DEFINITION: store knowledge\n");
	sc_cb_concat(cb, "- cbot what is SOMETHING: return DEFINITION\n");
	sc_cb_concat(cb, "- cbot who taugth you about SOMETHING: cbot is a "
	                 "tattle tale!\n");
}

struct cbot_plugin_ops ops = {
	.description = "allows the bot to remember things",
	.load = load,
	.help = help,
};
