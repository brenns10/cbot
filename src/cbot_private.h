/**
 * cbot_private.h
 */

#ifndef CBOT_PRIVATE_H
#define CBOT_PRIVATE_H

#include <inttypes.h>
#include <stdarg.h>

#include "libstephen/list.h"
#include "libstephen/re.h"

#include "cbot/cbot.h"

struct cbot_handler_list {
	cbot_handler_t *handler;
	size_t num;
	size_t alloc;
};

struct cbot {
	const char *name;
	struct cbot_handler_list hlists[_CBOT_NUM_EVENT_TYPES_];
	void *backend;
	struct cbot_actions actions;
	uint8_t hash[20];
};

struct cbot *cbot_create(const char *name);
void cbot_delete(struct cbot *obj);
void cbot_handle_event(struct cbot *bot, struct cbot_event event);
void cbot_handle_channel_message(struct cbot *bot, const char *channel,
                                 const char *user, const char *message);
void cbot_register(struct cbot *bot, enum cbot_event_type type,
                   cbot_handler_t handler);
void cbot_load_plugins(struct cbot *bot, char *plugin_dir, smb_iter names);

int cbot_is_authorized(struct cbot *cbot, const char *message);

#endif // CBOT_PRIVATE_H
