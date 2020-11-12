/**
 * cbot/db.h: API of the CBOTDB sqlite library / ORM / something-or-other.
 *
 * What it is:
 *
 *   This is a set of macros which help you "declare" your database accessor
 *   functions and bind them to the structs (objects) which they correspond to.
 *   The macros handle the sqlite boilerplate code, properly performing error
 *   handling, etc. They allow you to simply write your query, declare a
 *   function wrapping that query, and specify how the function's arguments are
 *   bound to the query, and how the function returns its results.
 *
 * How it works:
 *
 *   To use this library, you must include the header file. Then, create a
 *   function skeleton. The function can be named whatever you would like. It
 *   should return either an integer (which will be a status variable) or a
 *   pointer to some struct you have defined. The parameters to the function can
 *   be whatever you like, as you read the documentation you'll get an idea what
 *   they need to be. THIS FUNCTION BODY SHOULD CONTAIN NO CUSTOM CODE. It
 *   should only contain CBOTDB macro calls, in the specified order:
 *
 *   (1) First, a call to CBOTDB_QUERY_FUNC_BEGIN(). See its docs for details.
 *   The most important thing to include in this function call is the query
 *   string. Write any SQL query, and if you need to bind names in the query,
 *   use sqlite's $name syntax.
 *
 *   (2) Second, a list of bindings (see section Binding functions below). The
 *   bindings will map function arguments to identifiers within the query (which
 *   will have the same name). If there are no bindings to be made, use
 *   CBOTDB_NOBIND().
 *
 *   (3) Finally, declare a "CBOTDB_[SOMETHING]_RESULT()" to end the function.
 *   This will conclude a query and return its result. For complex queries (e.g.
 *   ones which return a struct pointer, or a list of structs) you will need to
 *   further specify column mappings in your RESULT() function.
 *
 * Examples? See src/db.c for functions using CBOTDB.
 */
#ifndef CBOT_DB_H
#define CBOT_DB_H

#include <string.h>

#include <sc-collections.h>
#include <sqlite3.h>

#include "cbot/cbot.h"

/******
 * [internal] Binding macros/functions: these should be named:
 *
 *     cbot_sqlite3_bind_TYPENAME(stmt, index, data)
 *
 * These are used in the macros which bind parameter names to variables /
 * parameters of the function.  The cbot_sqlite_bind(type) macro will expand to
 * the proper macro/function name, see below.
 */

static inline int cbot_sqlite3_bind_text(struct sqlite3_stmt *stmt, int index,
                                         char *data)
{
	return sqlite3_bind_text(stmt, index, data, -1, SQLITE_STATIC);
}

#define cbot_sqlite3_bind_int    sqlite3_bind_int
#define cbot_sqlite3_bind_int64  sqlite3_bind_int64
#define cbot_sqlite3_bind_double sqlite3_bind_double

/******
 * [internal] Selects the proper binding macro/function.
 */
#define _cbot_sqlite_bind(type) cbot_sqlite3_bind_##type
#define cbot_sqlite_bind(type)  _cbot_sqlite_bind(type)

/******
 * [internal] Column accessor macros: these should be named:
 *
 *     cbot_sqlite3_column_TYPENAME(stmt, index)
 *
 * The macro should evaluate to an expression of the correct C type for the
 * sqlite type. The cbot_sqlite3_column(type) macro will expand to the proper
 * macro/function name, see below.
 */
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

/******
 * [internal] Selects the proper column accessor macro/function
 */
#define _cbot_sqlite_column(type) cbot_sqlite3_column_##type
#define cbot_sqlite_column(type)  _cbot_sqlite_column(type)

/******
 * Miscellaneous public APIs which don't belong to a category
 */

/**
 * @brief Declare an output for use with CBOTDB_LIST_RESULT
 * @param type The sqlite type of the result tuple
 * @param column_index The index into the result tuple
 * @param struct_field The name of the struct field to assign it to
 */
#define CBOTDB_OUTPUT(type, column_index, struct_field)                        \
	STRUCT->struct_field = cbot_sqlite_column(type)(STMT, column_index)

/**
 * @brief Declare the header of a CBOTDB query function.
 *
 * This function MUST be at the beginning of any CBOTDB query function. No
 * exceptions.
 *
 * @param bot The bot instance for the query function
 * @param result_type The result type used for struct / list queries
 * @param query_str The query for this function
 */
#define CBOTDB_QUERY_FUNC_BEGIN(bot, result_type, query_str)                   \
	char *QUERY = query_str;                                               \
	result_type *STRUCT = NULL;                                            \
	sqlite3_stmt *STMT = NULL;                                             \
	int COUNT = 0;                                                         \
	int RV;                                                                \
	(void)STRUCT; /* mark unused to shut up compiler */                    \
	(void)COUNT;  /* mark unused to shut up compiler */                    \
	RV = sqlite3_prepare_v2(cbot_db_conn(bot), QUERY, -1, &STMT, NULL);    \
	if (RV != SQLITE_OK) {                                                 \
		fprintf(stderr, "prepare(%s): %s(%d): %s\n", __func__,         \
		        sqlite3_errstr(RV), RV,                                \
		        sqlite3_errmsg(cbot_db_conn(bot)));                    \
		goto OUT_LABEL;                                                \
	}

/******
 * Binding functions
 */

/**
 * @brief No binding is done for this query
 * This should be used if and only if CBOTDB_BIND_ARG() is used nowhere in a
 * query function.
 */
#define CBOTDB_NOBIND()

/**
 * @brief Bind an argument to a query.
 * @param sqlite_type the sqlite type of the argument
 * @param name the name of the argument. Note that this is both the name of the
 *   C variable containing the argument, but also the $name of the alias used in
 *   the query string.
 */
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

/******
 * Result functions.
 * After the initial CBOTDB_QUERY_FUNC_BEGIN(), and some combination of binding
 * function, use one of these functions to terminate your query function.
 */

/**
 * @brief End a function which returns a pointer to single struct.
 * @param output_stmts a semicolon-separated sequence of CBOTDB_OUTPUT macro
 *   calls, each one declaring an assignment to a field of the struct.
 * @returns The pointer return value will be a malloc() allocated pointer to the
 *   struct of whatever type was declared in CBOTDB_QUERY_FUNC_BEGIN().
 */
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
	goto OUT_LABEL; /* try to avoid unused label warning */                \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return STRUCT;

/**
 * @brief End a function which inserts a row
 * @param bot The bot instance being used
 * @returns The integer return value of the query function will be the rowid of
 *   the inserted row, or else negative on error.
 */
#define CBOTDB_INSERT_RESULT(bot)                                              \
	RV = sqlite3_step(STMT);                                               \
	if (RV == SQLITE_DONE) {                                               \
		RV = sqlite3_last_insert_rowid(cbot_db_conn(bot));             \
	} else {                                                               \
		fprintf(stderr, "step: %d\n", RV);                             \
		RV = -1;                                                       \
	}                                                                      \
	goto OUT_LABEL; /* try to avoid unused label warning */                \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

/**
 * @brief End a function which should have no result.
 * @returns The integer return value of the query function will be 0 on success,
 *   negative on failure.
 */
#define CBOTDB_NO_RESULT()                                                     \
	RV = sqlite3_step(STMT);                                               \
	if (RV == SQLITE_DONE) {                                               \
		RV = 0;                                                        \
	} else {                                                               \
		fprintf(stderr, "step: %d\n", RV);                             \
		RV = -1;                                                       \
	}                                                                      \
	goto OUT_LABEL; /* try to avoid unused label warning */                \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

/**
 * @brief End a function which returns a single integer value.
 * @returns The integer return value of the query function will be negative on
 *   failure, otherwise it will be the integer result from the query. This
 *   function ought not to be used if the query result could be negative, as
 *   there will be no way to tell the difference between a negative result and
 *   an error in the query. In cases where negative is allowed, use
 *   CBOTDB_SINGLE_INTPTR_RESULT().
 */
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
	goto OUT_LABEL; /* try to avoid unused label warning */                \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

/**
 * @brief End a function which returns a single integer value, into a pointer.
 * @param ptr Name of the pointer to store the result into
 * @returns The integer return value of the query function will be negative on
 *   failure, otherwise it will be 0 and ptr will be set.
 */
#define CBOTDB_SINGLE_INTPTR_RESULT(ptr)                                       \
	RV = sqlite3_step(STMT);                                               \
	if (RV == SQLITE_DONE) {                                               \
		RV = -1;                                                       \
	} else if (RV == SQLITE_ROW) {                                         \
		RV = 0;                                                        \
		*ptr = sqlite3_column_int(STMT, 0);                            \
	} else {                                                               \
		fprintf(stderr, "step: %d\n", RV);                             \
		RV = -1;                                                       \
	}                                                                      \
	goto OUT_LABEL; /* try to avoid unused label warning */                \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

/**
 * @brief End a function which should return a linked list of values.
 * @param bot The cbot instance
 * @param head_p A pointer to a struct sc_list_head which will be used to store
 *   the results. This should be an argument to the function.
 */
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
	goto OUT_LABEL; /* try to avoid unused label warning */                \
	OUT_LABEL:                                                             \
	sqlite3_finalize(STMT);                                                \
	return RV;

#endif
