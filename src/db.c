#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "cbot/cbot.h"
#include "cbot_private.h"

static inline int cbot_sqlite3_bind_text(struct sqlite3_stmt *stmt, int index,
                                         char *data)
{
	return sqlite3_bind_text(stmt, index, data, -1, SQLITE_STATIC);
}

#define cbot_sqlite3_bind_int    sqlite3_bind_int
#define cbot_sqlite3_bind_int64  sqlite3_bind_int64
#define cbot_sqlite3_bind_double sqlite3_bind_double

#define _cbot_sqlite_bind(type) cbot_sqlite3_bind_##type
#define cbot_sqlite_bind(type)  _cbot_sqlite_bind(type)

static inline char *cbot_sqlite3_column_text(struct sqlite3_stmt *stmt,
                                             int index)
{
	char *res = (char *)sqlite3_column_text(stmt, index);
	if (res)
		res = strdup(res);
	return res;
}

#define cbot_sqlite3_column_int    sqlite3_column_int
#define cbot_sqlite3_column_int64  sqlite3_column_int64
#define cbot_sqlite3_column_double sqlite3_column_double

#define _cbot_sqlite_column(type) cbot_sqlite3_column_##type
#define cbot_sqlite_column(type)  _cbot_sqlite_column(type)

#define CBOTDB_OUTPUT(type, column_index, struct_field)                        \
	STRUCT->struct_field = cbot_sqlite_column(type)(STMT, column_index)

#define CBOTDB_NOBIND() goto OUT_LABEL;

#define CBOTDB_BIND_ARG(sqlite_type, name)                                     \
	RV = sqlite3_bind_parameter_index(STMT, "$" #name);                    \
	if (RV == 0) {                                                         \
		fprintf(stderr, "sqlite3_bind_parameter_index (%s): %d\n",     \
		        #name, RV);                                            \
		RV = -1;                                                       \
		goto OUT_LABEL;                                                \
	}                                                                      \
	RV = cbot_sqlite_bind(sqlite_type)(STMT, RV, name);                    \
	if (RV != SQLITE_OK) {                                                 \
		fprintf(stderr, "sqlite3_bind: %d\n", RV);                     \
		RV = -1;                                                       \
		goto OUT_LABEL;                                                \
	}

#define CBOTDB_QUERY_FUNC_BEGIN(bot, result_type, query_str)                   \
	char *QUERY = query_str;                                               \
	result_type *STRUCT = NULL;                                            \
	sqlite3_stmt *STMT = NULL;                                             \
	int COUNT = 0;                                                         \
	int RV;                                                                \
	(void)STRUCT; /* mark unused to shut up compiler */                    \
	(void)COUNT;  /* mark unused to shut up compiler */                    \
	RV = sqlite3_prepare_v2(bot->privDb, QUERY, -1, &STMT, NULL);          \
	if (RV != SQLITE_OK) {                                                 \
		fprintf(stderr, "prepare(%s): %s(%d): %s\n", __func__,         \
		        sqlite3_errstr(RV), RV, sqlite3_errmsg(bot->privDb));  \
		return -1;                                                     \
	}

#define CBOTDB_SINGLE_STRUCT_RESULT(output_stmts)                              \
	RV = sqlite3_step(STMT);                                               \
	if (RV == SQLITE_DONE) {                                               \
		RV = -1;                                                       \
	} else if (RV == SQLITE_ROW) {                                         \
		RV = 0;                                                        \
		STRUCT = calloc(1, sizeof(*STRUCT));                           \
		output_stmts;                                                  \
	} else {                                                               \
		fprintf(stderr, "step: %d\n", RV);                             \
		RV = -1;                                                       \
	}                                                                      \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return STRUCT;

#define CBOTDB_INSERT_RESULT(bot)                                              \
	RV = sqlite3_step(STMT);                                               \
	if (RV == SQLITE_DONE) {                                               \
		RV = sqlite3_last_insert_rowid(bot->privDb);                   \
	} else {                                                               \
		fprintf(stderr, "step: %d\n", RV);                             \
		RV = -1;                                                       \
	}                                                                      \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

#define CBOTDB_NO_RESULT()                                                     \
	RV = sqlite3_step(STMT);                                               \
	if (RV == SQLITE_DONE) {                                               \
		RV = 0;                                                        \
	} else {                                                               \
		fprintf(stderr, "step: %d\n", RV);                             \
		RV = -1;                                                       \
	}                                                                      \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

#define CBOTDB_SINGLE_INTEGER_RESULT()                                         \
	RV = sqlite3_step(STMT);                                               \
	if (RV == SQLITE_DONE) {                                               \
		RV = -1;                                                       \
	} else if (RV == SQLITE_ROW) {                                         \
		RV = sqlite3_column_int(STMT, 0);                              \
	} else {                                                               \
		fprintf(stderr, "step: %d\n", RV);                             \
		RV = -1;                                                       \
	}                                                                      \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

#define CBOTDB_LIST_RESULT(bot, head_p, output_stmts)                          \
	while ((RV = sqlite3_step(STMT)) == SQLITE_ROW) {                      \
		STRUCT = calloc(1, sizeof(*STRUCT));                           \
		output_stmts;                                                  \
		sc_list_insert_end(head_p, &STRUCT->list);                     \
		COUNT += 1;                                                    \
	}                                                                      \
	if (RV == SQLITE_DONE) {                                               \
		RV = COUNT;                                                    \
	} else {                                                               \
		RV = -1;                                                       \
	}                                                                      \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

#define CBOTDB_UPSERT_FUNC(name, getter, inserter)                             \
	int name(struct cbot *bot, char *ARG)                                  \
	{                                                                      \
		int RV = getter(bot, ARG);                                     \
		if (RV > 0)                                                    \
			return RV;                                             \
		else                                                           \
			return inserter(bot, ARG);                             \
	}

static int cbot_db_get_user_id(struct cbot *bot, char *nick)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "SELECT id FROM user WHERE nick=$nick;");
	CBOTDB_BIND_ARG(text, nick);
	CBOTDB_SINGLE_INTEGER_RESULT();
}

static int cbot_db_insert_user(struct cbot *bot, char *nick)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "INSERT INTO user(nick) VALUES($nick);");
	CBOTDB_BIND_ARG(text, nick);
	CBOTDB_INSERT_RESULT(bot);
}

static CBOTDB_UPSERT_FUNC(cbot_db_upsert_user, cbot_db_get_user_id,
                          cbot_db_insert_user);

int cbot_db_get_chan_id(struct cbot *bot, char *name)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "SELECT id FROM channel WHERE name=$name;");
	CBOTDB_BIND_ARG(text, name);
	CBOTDB_SINGLE_INTEGER_RESULT();
}

int cbot_db_insert_chan(struct cbot *bot, char *chan)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "INSERT INTO channel(name) VALUES($chan);");
	CBOTDB_BIND_ARG(text, chan);
	CBOTDB_INSERT_RESULT(bot);
}

static CBOTDB_UPSERT_FUNC(cbot_db_upsert_chan, cbot_db_get_chan_id,
                          cbot_db_insert_chan);

int cbot_db_upsert_membership(struct cbot *bot, int user_id, int chan_id)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "INSERT INTO membership(user_id, channel_id) "
	                        "VALUES($user_id, $chan_id) "
	                        "ON CONFLICT DO NOTHING;");
	CBOTDB_BIND_ARG(int, user_id);
	CBOTDB_BIND_ARG(int, chan_id);
	CBOTDB_NO_RESULT();
}

int cbot_get_members(struct cbot *bot, char *chan, struct sc_list_head *head)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, struct cbot_user_info,
	                        "SELECT u.nick, u.realname "
	                        "FROM user u "
	                        " INNER JOIN membership m ON u.id=m.user_id "
	                        " INNER JOIN channel c ON c.id=m.channel_id "
	                        "WHERE c.name = $chan;");
	CBOTDB_BIND_ARG(text, chan);
	CBOTDB_LIST_RESULT(bot, head, CBOTDB_OUTPUT(text, 0, username);
	                   CBOTDB_OUTPUT(text, 1, realname););
}

int cbot_clear_channel_memberships(struct cbot *bot, char *chan)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "DELETE FROM membership "
	                        " WHERE channel_id in ("
	                        "  SELECT c.id FROM channel c WHERE name=$chan"
	                        ");");
	CBOTDB_BIND_ARG(text, chan);
	CBOTDB_NO_RESULT();
}

int cbot_set_channel_topic(struct cbot *bot, char *chan, char *topic)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "INSERT INTO channel(name, topic) "
	                        "VALUES($chan, $topic) "
	                        "ON CONFLICT(name) DO UPDATE "
	                        "SET topic=excluded.topic;");
	CBOTDB_BIND_ARG(text, chan);
	CBOTDB_BIND_ARG(text, topic);
	CBOTDB_NO_RESULT();
}

void cbot_user_info_free(struct cbot_user_info *info)
{
	free(info->username);
	if (info->realname)
		free(info->realname);
	free(info);
}

void cbot_user_info_free_all(struct sc_list_head *head)
{
	struct cbot_user_info *info, *next;
	sc_list_for_each_safe(info, next, head, list, struct cbot_user_info)
	{
		cbot_user_info_free(info);
	}
}

int cbot_add_membership(struct cbot *bot, char *nick, char *chan)
{
	int user_id = cbot_db_upsert_user(bot, nick);
	int chan_id = cbot_db_upsert_chan(bot, chan);
	int rv;
	if (user_id < 0 || chan_id < 0)
		return -1;
	rv = cbot_db_upsert_membership(bot, user_id, chan_id);
	return rv;
}

int cbot_clear_memberships(struct cbot *bot)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void, "DELETE FROM membership;");
	CBOTDB_NOBIND();
	CBOTDB_NO_RESULT();
}

/******
 * Table registration and migration
 */

int create_schema_registry(struct cbot *bot)
{
	int rv;
	char *errmsg = NULL;
	char *stmts = "CREATE TABLE IF NOT EXISTS cbot_schema_registry ( "
	              " id INTEGER PRIMARY KEY ASC, "
	              " name TEXT NOT NULL UNIQUE, "
	              " version INTEGER NOT NULL "
	              "); ";
	rv = sqlite3_exec(bot->privDb, stmts, NULL, NULL, &errmsg);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "sqlite error creating tables: %s\n", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	return 0;
}

/*
 * returns -1 if the table does not exist, returns -2 on error
 */
static int get_schema_version(struct cbot *bot, char *name)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "SELECT version FROM cbot_schema_registry "
	                        "WHERE name=$name;");
	CBOTDB_BIND_ARG(text, name);
	CBOTDB_SINGLE_INTEGER_RESULT();
}

static int query_and_update_schema_version(struct cbot *bot, const char *name,
                                           unsigned int version,
                                           const char *query)
{
	int rv = 0;
	char *errmsg = NULL;
	struct sc_charbuf cb;
	sc_cb_init(&cb, 1024);

	sc_cb_printf(&cb,
	             "BEGIN TRANSACTION; %s "
	             "INSERT INTO cbot_schema_registry(name, version) "
	             "VALUES (\"%s\", %u) ON CONFLICT(name) DO UPDATE "
	             "SET version=excluded.version; "
	             "COMMIT;",
	             query, name, version);

	rv = sqlite3_exec(bot->privDb, cb.buf, NULL, NULL, &errmsg);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "table registration error: %s\n", errmsg);
		sqlite3_free(errmsg);
		rv = -1;
		goto out;
	}
	rv = 0;

out:
	sc_cb_destroy(&cb);
	return rv;
}

int cbot_db_register_internal(struct cbot *bot, const struct cbot_db_table *tbl)
{
	int s_ver = get_schema_version(bot, (char *)tbl->name);
	int rv;
	unsigned int u_ver;

	if (s_ver < 0) {
		printf("cbot_db: create table \"%s\" version %u\n", tbl->name,
		       tbl->version);
		rv = query_and_update_schema_version(bot, tbl->name,
		                                     tbl->version, tbl->create);
		return rv;
	}

	u_ver = (unsigned int)s_ver;
	if (u_ver > tbl->version) {
		fprintf(stderr,
		        "table %s has newer version (%u) than supported (%u)\n",
		        tbl->name, u_ver, tbl->version);
		return -1;
	} else if (u_ver == tbl->version) {
		printf("cbot_db: table \"%s\" version %u is up-to-date\n",
		       tbl->name, tbl->version);
		return 0;
	}

	for (; u_ver < tbl->version; u_ver++) {
		printf("cbot_db: alter table \"%s\" from version %u to %u\n",
		       tbl->name, u_ver, u_ver + 1);
		rv = query_and_update_schema_version(bot, tbl->name, u_ver + 1,
		                                     tbl->alters[u_ver]);
		if (rv < 0) {
			return rv;
		}
	}

	return 0;
}

/*
 * In case we would like to allow plugin-specific processing, this allows us
 * to do it. The internal function above should be used for non-plugin
 * processing.
 */
int cbot_db_register(struct cbot_plugin *plugin,
                     const struct cbot_db_table *tbl)
{
	return cbot_db_register_internal(plugpriv(plugin)->bot, tbl);
}

/******
 * Initialization and exit routines.
 */

const char *tbl_user_alters[] = {};

struct cbot_db_table tbl_user = {
	.name = "user",
	.version = 0,
	.create = "CREATE TABLE user ( "
	          " id INTEGER PRIMARY KEY ASC, "
	          " nick TEXT NOT NULL UNIQUE, "
	          " realname TEXT, "
	          " host TEXT "
	          ");",
	.alters = tbl_user_alters,
};

const char *tbl_channel_alters[] = {};

struct cbot_db_table tbl_channel = {
	.name = "channel",
	.version = 0,
	.create = "CREATE TABLE channel ( "
	          " id INTEGER PRIMARY KEY ASC, "
	          " name TEXT NOT NULL UNIQUE, "
	          " topic TEXT "
	          ");",
	.alters = tbl_channel_alters,
};

const char *tbl_membership_alters[] = {};

const struct cbot_db_table tbl_membership = {
	.name = "membership",
	.version = 0,
	.create = "CREATE TABLE membership ( "
	          " user_id INT NOT NULL, "
	          " channel_id INT NOT NULL, "
	          " UNIQUE(user_id, channel_id) "
	          ");",
	.alters = tbl_membership_alters,
};

int cbot_db_init(struct cbot *bot)
{
	int rv;
	rv = sqlite3_open_v2(bot->db_file, &bot->privDb,
	                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rv != SQLITE_OK) {
		return -1;
	}

	rv = create_schema_registry(bot);
	if (rv < 0) {
		return rv;
	}

	rv = cbot_db_register_internal(bot, &tbl_user);
	if (rv < 0)
		return rv;

	rv = cbot_db_register_internal(bot, &tbl_channel);
	if (rv < 0)
		return rv;

	rv = cbot_db_register_internal(bot, &tbl_membership);
	if (rv < 0)
		return rv;

	rv = cbot_clear_memberships(bot);
	if (rv < 0)
		return rv;

	return 0;
}
