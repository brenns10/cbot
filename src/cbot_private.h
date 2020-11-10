/**
 * cbot_private.h
 */

#ifndef CBOT_PRIVATE_H
#define CBOT_PRIVATE_H

#include <inttypes.h>
#include <stdarg.h>

#include <libconfig.h>
#include <sc-collections.h>
#include <sc-lwt.h>
#include <sc-regex.h>
#include <sqlite3.h>

#include "cbot/cbot.h"

struct cbot_plugpriv;

struct cbot_handler {
	/* Function called by CBot */
	cbot_handler_t handler;
	/* User data for the handler */
	void *user;
	/* Plugin owning this */
	struct cbot_plugpriv *plugin;
	/* Optionally, a regex which must match in order to be called. */
	struct sc_regex *regex;
	/* List containing all handlers for this event. */
	struct sc_list_head handler_list;
	/* List containing all handlers for this plugin. */
	struct sc_list_head plugin_list;
};

struct cbot_plugpriv {
	struct cbot_plugin p;
	/* Name of the plugin */
	char *name;
	/* Redundant, but the plugin could modify p.bot */
	struct cbot *bot;
	/* List of handlers provided */
	struct sc_list_head handlers;
	/* List of plugins */
	struct sc_list_head list;
	/* dlopen handle */
	void *handle;
};

struct cbot_backend_ops {
	const char *name;
	int (*configure)(struct cbot *cbot, config_setting_t *group);
	void (*run)(struct cbot *cbot);
	void (*send)(const struct cbot *cbot, const char *to, const char *msg);
	void (*me)(const struct cbot *cbot, const char *to, const char *msg);
	void (*op)(const struct cbot *cbot, const char *channel,
	           const char *username);
	void (*join)(const struct cbot *cbot, const char *channel,
	             const char *password);
	void (*nick)(const struct cbot *cbot, const char *newnick);
};

extern struct cbot_backend_ops irc_ops;
extern struct cbot_backend_ops cli_ops;

struct cbot_channel_conf {
	char *name;
	char *pass;
	struct sc_list_head list;
};

struct cbot {
	/* Loaded from configuration */
	char *name;
	char *backend_name;
	char *plugin_dir;
	char *db_file;
	struct sc_list_head init_channels;

	struct sc_list_head handlers[_CBOT_NUM_EVENT_TYPES_];
	struct sc_list_head plugins;
	uint8_t hash[20];
	struct cbot_backend_ops *backend_ops;
	void *backend;
	sqlite3 *privDb;
	struct sc_lwt_ctx *lwt_ctx;
	struct sc_lwt *lwt;
};

struct cbot *cbot_create(void);
int cbot_load_config(struct cbot *bot, const char *conf_file);
void cbot_run(struct cbot *bot);
void cbot_delete(struct cbot *obj);

int cbot_is_authorized(struct cbot *cbot, const char *message);

void cbot_set_nick(struct cbot *bot, const char *newname);

/* Functions which backends can call, to trigger various types of events */
void cbot_handle_message(struct cbot *bot, const char *channel,
                         const char *user, const char *message, bool action);
void cbot_handle_user_event(struct cbot *bot, const char *channel,
                            const char *user, enum cbot_event_type type);
void cbot_handle_nick_event(struct cbot *bot, const char *old_username,
                            const char *new_username);

void *base64_decode(const char *str, int explen);

/*******
 * Database functions!
 *******/
int cbot_add_membership(struct cbot *bot, char *nick, char *chan);
int cbot_clear_channel_memberships(struct cbot *bot, char *chan);
int cbot_set_channel_topic(struct cbot *bot, char *chan, char *topic);
int cbot_db_create_tables(struct cbot *bot);

#define nelem(arr) (sizeof(arr) / sizeof(arr[0]))

void run_cbot_irc(int argc, char *argv[]);
void run_cbot_cli(int argc, char **argv);

#endif // CBOT_PRIVATE_H
