/**
 * cbot_private.h
 */

#ifndef CBOT_PRIVATE_H
#define CBOT_PRIVATE_H

#include <inttypes.h>
#include <stdarg.h>

#include "libstephen/list.h"
#include "sc-collections.h"
#include "sc-regex.h"

#include "cbot/cbot.h"

struct cbot_handler {
	/* Function called by CBot */
	cbot_handler_t handler;
	/* User data for the handler */
	void *user;
	/* Optionally, a regex which must match in order to be called. */
	struct sc_regex *regex;
	/* List containing all handlers for this event. */
	struct sc_list_head handler_list;
	/* List containing all handlers for this plugin. */
	struct sc_list_head plugin_list;
};

struct cbot_plugin {
	/* Name of the plugin */
	char *name;
	/* List of handlers provided */
	struct sc_list_head handlers;
	/* List of plugins */
	struct sc_list_head plugins;
};

struct cbot_backend {
	void (*send)(const struct cbot *cbot, const char *to, const char *msg);
	void (*me)(const struct cbot *cbot, const char *to, const char *msg);
	void (*op)(const struct cbot *cbot, const char *channel,
	           const char *username);
	void (*join)(const struct cbot *cbot, const char *channel,
	             const char *password);
	void *backend_data;
};

struct cbot {
	const char *name;
	struct sc_list_head handlers[_CBOT_NUM_EVENT_TYPES_];
	uint8_t hash[20];
	struct cbot_backend *backend;
};

struct cbot *cbot_create(const char *name, struct cbot_backend *backend);
void cbot_delete(struct cbot *obj);
void cbot_load_plugins(struct cbot *bot, char *plugin_dir, smb_iter names);

int cbot_is_authorized(struct cbot *cbot, const char *message);

/* Functions which backends can call, to trigger various types of events */
void cbot_handle_message(struct cbot *bot, const char *channel,
                         const char *user, const char *message, bool action);
void cbot_handle_user_event(struct cbot *bot, const char *channel,
                            const char *user, enum cbot_event_type type);
void cbot_handle_nick_event(struct cbot *bot, const char *old_username,
                            const char *new_username);

#endif // CBOT_PRIVATE_H
