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
		fprintf(stderr, "prepare: %s(%d): %s\n", sqlite3_errstr(RV),   \
		        RV, sqlite3_errmsg(bot->privDb));                      \
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

int cbot_db_create_tables(struct cbot *bot)
{
	int rv;
	char *errmsg = NULL;
	char *stmts = "CREATE TABLE user ( "
	              " id INTEGER PRIMARY KEY ASC, "
	              " nick TEXT NOT NULL UNIQUE, "
	              " realname TEXT, "
	              " host TEXT "
	              "); "
	              "CREATE TABLE channel ( "
	              " id INTEGER PRIMARY KEY ASC, "
	              " name TEXT NOT NULL UNIQUE, "
	              " topic TEXT "
	              "); "
	              "CREATE TABLE membership ( "
	              " user_id INT NOT NULL, "
	              " channel_id INT NOT NULL, "
	              " UNIQUE(user_id, channel_id) "
	              "); ";
	rv = sqlite3_exec(bot->privDb, stmts, NULL, NULL, &errmsg);
	if (rv != SQLITE_OK) {
		fprintf(stderr, "sqlite error creating tables: %s\n", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	return 0;
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
