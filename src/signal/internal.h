/*
 * signal/internal.h: shared definitions related to Signal backends
 */
#ifndef CBOT_SIGNALD_INTERNAL_DOT_H
#define CBOT_SIGNALD_INTERNAL_DOT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <libconfig.h>
#include <nosj.h>
#include <sc-collections.h>
#include <sys/types.h>

#include "cbot/cbot.h"

struct cbot_signal_backend;

/** Reaction callback information */
struct signal_reaction_cb {
	/** Timestamp of the message to monitor for reactions */
	uint64_t ts;
	/** Operations from the plugin */
	struct cbot_reaction_ops ops;
	/** Argument to plugin */
	void *arg;
};

/** Signal's representation of a @mention */
struct signal_mention {
	/** UTF-16 index of the start of the text replaced by @mention */
	uint64_t start;
	/** UTF-16 length of the text to replace */
	uint64_t length;
	/** UUID of the user mentioned */
	char *uuid;
};

/** Operations that are specific to a Signal API bridge. */
struct signal_bridge_ops {
	/** Send an already-quoted direct message */
	uint64_t (*send_single)(struct cbot_signal_backend *, const char *to,
	                        const char *quoted_msg,
	                        const struct signal_mention *, size_t);
	/** Send an already-quoted message to a group */
	uint64_t (*send_group)(struct cbot_signal_backend *, const char *to,
	                       const char *quoted_msg,
	                       const struct signal_mention *, size_t);
	/** Update profile name */
	void (*nick)(const struct cbot *bot, const char *newnick);
	/** Run the bot backend thread */
	void (*run)(struct cbot *bot);
	/** Bridge-specific configuration routine */
	int (*configure)(struct cbot *bot, config_setting_t *group);
};

/*
 * We have two current bridges: Signald and Signal-CLI
 * https://signald.org/
 * https://github.com/AsamK/signal-cli
 */
extern struct signal_bridge_ops signald_bridge;
extern struct signal_bridge_ops signalcli_bridge;

struct cbot_signal_backend {
	/* Signal bridge operations */
	struct signal_bridge_ops *bridge;

	/* File descriptor for our signal bridge */
	int fd;

	int write_fd; /* Additional descriptor for bridge (ignore if 0) */
	pid_t child;

	/* Queued messages ready to read */
	struct sc_list_head messages;
	char *spill;
	int spilllen;

	/* Ignore DMs? (Useful for running multiple bots on the same acct) */
	int ignore_dm;

	/*
	 * A stdio write stream associated with the above socket. It is in
	 * unbuffered mode, used to write formatted JSON commands.
	 */
	FILE *ws;

	/* Phone number & uuid of the bot sender */
	char *sender;
	char *uuid;

	/* uuid of authorized user */
	char *auth_uuid;

	/* Reference to the bot */
	struct cbot *bot;

	/* Array of message timestamps and information on callbacks */
	struct sc_array pending;

	/* Queued messages to send */
	struct sc_list_head msgq;

	uint64_t id;
};

/***** jmsg.c *****/

/** Structure representing a line of text which is a JSON message. */
struct jmsg {
	/** Owns the line of text and parsed JSON metadata */
	struct json_easy easy;
	/** Links the messages together in the handling queue */
	struct sc_list_head list;
};

/**
 * Read the next jmsg from the queue of incoming messages. If there are no
 * messages in the queue, this will block.
 * @param sig Signal backend
 * @return NULL on error, otherwise a struct jmsg ready to use
 */
struct jmsg *jmsg_next(struct cbot_signal_backend *sig);

/**
 * Wait for a jmsg where @a field has value @a value
 * @param field The field name to wait on
 * @param value A value to wait for (only strings are supported)
 */
struct jmsg *jmsg_wait_field(struct cbot_signal_backend *sig, const char *field,
                             const char *value);

/**
 * Check if a message applies to any waiter. If so, deliver it
 * @param sig Signal backend
 * @param jm Message to deliver to waiter
 * @returns true if the message is delivered to a waiter. When this is the case,
 *   the waiter takes ownership of @a jm, and it must no longer be accessed by
 *   the caller.
 */
bool jmsg_deliver(struct cbot_signal_backend *sig, struct jmsg *jm);

/**
 * Free a JSON message object, in whatever lifetime state it may be.
 * @param jm Message to free.
 */
void jmsg_free(struct jmsg *jm);

/***** mention.c *****/

#define MENTION_ERR   0
#define MENTION_USER  1
#define MENTION_GROUP 2

/**
 * Formats a mention placeholder, without freeing old string.
 * @param string The value of the mention
 * @param prefix The prefix to apply (e.g. uuid, group)
 * @returns A newly allocated string with the mention text.
 */
char *mention_format_p(const char *string, const char *prefix);

/**
 * Formats a mention placeholder, freeing old string.
 * @param string The value of the mention (FREED by this function)
 * @param prefix The prefix to apply (e.g. uuid, group)
 * @returns A newly allocated string with the mention text.
 */
char *mention_format(char *string, const char *prefix);

/**
 * Parse a mention placeholder text.
 * @param string Input text starting at the mention
 * @param[out] kind The kind of message: MENTION_ERR for error.
 * @param[out] offset If provided, will be filled with the number of characters
 * of thismention.
 * @return The value of the mention (a new string which must be freed)
 */
char *mention_parse(const char *string, int *kind, int *offset);

/**
 * Return a newly allocated string with mentions "replaced"
 *
 * Signald gives us messages with mentions in a strange format. The mentions
 * come in a JSON array, and their "start" field doesn't seem accurate. However,
 * each mention is replaced with MENTION_PLACEHOLDER, so we simply iterate over
 * each placeholder, grab a mention from the JSON list, and insert our
 * placeholder:
 *
 *   @(uuid:blah)
 *
 * Our placeholder can be translated back at the end (see below). To preserve
 * mentions which may contain @, we also identify the @ sign and double it.
 * @param str Message string
 * @param je The JSON message buffer
 * @param list The index of the array to read menitons from
 * @return A newly allocated string with mentions expanded to placeholders.
 */
char *mention_from_json(const char *str, struct json_easy *je, uint32_t list);

/**
 * Return a newly allocated string with necessary escaping for JSON. Return an
 * array which contains all mention JSON elements.
 *
 * This is called with a message text just before sending it.
 *
 * Beyond obvious JSON escaping, this function detects any mention placeholder
 * mention text:
 *   @(uuid:UUUID)
 * That text is removed and a JSON array element is created in "mentions" to
 * represent it.
 *
 * Duplicated "@@" are resolved back to "@" - this is to reverse the escaping
 * done by mention_from_json() above.
 *
 * @param instr Input string
 * @param[out] ms Will be set to an array of mention objects parsed
 * @param[out] n Will be set to the length of @a ms
 */
char *json_quote_and_mention(const char *instr, struct signal_mention **ms,
                             size_t *n);

/**
 * Return a newly allocated string with necessary JSON escaping.
 * No handling of mentions is done. If there's a "mention" text, it will be
 * left as-is.
 */
char *json_quote_nomention(const char *instr);

/***** backend.c *****/

/**
 * Fetch the reaction callback for a given message timestamp
 * @param sig Signal backend
 * @param ts Message timestamp
 * @param[out] out Structure filled with details if found
 * @returns true if a reaction callback was found for the message
 */
bool signal_get_reaction_cb(struct cbot_signal_backend *sig, uint64_t ts,
                            struct signal_reaction_cb *out);

/**
 * Return true if the bot is listening to a group and we shoul handle messages
 * @param sig Signal backend
 * @param group Group ID (not in @(group:foo) format)
 * @returns true if we should handle the message
 */
bool signal_is_group_listening(struct cbot_signal_backend *sig,
                               const char *group);

#endif // CBOT_SIGNALD_INTERNAL_DOT_H
