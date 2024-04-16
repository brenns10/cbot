#ifndef CBOT_SIGNALD_INTERNAL_DOT_H
#define CBOT_SIGNALD_INTERNAL_DOT_H

#include <libconfig.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <cbot/cbot.h>
#include <nosj.h>
#include <sc-collections.h>
#include <time.h>

struct signal_user;
struct cbot_signal_backend;

struct signal_reaction_cb {
	/* Timestamp of the message which we sent, that may get reacted */
	uint64_t ts;
	/* Operations from the plugin */
	struct cbot_reaction_ops ops;
	/* Argument to plugin */
	void *arg;
};

struct signal_mention {
	uint64_t start;
	uint64_t length;
	char *uuid;
};

struct signal_bridge_ops {
	uint64_t (*send_single)(struct cbot_signal_backend *, const char *to,
	                        const char *quoted_msg,
	                        const struct signal_mention *, size_t);
	uint64_t (*send_group)(struct cbot_signal_backend *, const char *to,
	                       const char *quoted_msg,
	                       const struct signal_mention *, size_t);
	void (*nick)(const struct cbot *bot, const char *newnick);
	void (*run)(struct cbot *bot);
	int (*configure)(struct cbot *bot, config_setting_t *group);
};

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

/*
 * Structure representing a line of text which is a JSON message.
 * Owns the orig and tok pointers (though tok may be null).
 * Can be parsed, and then subsequent lookup operations can happen.
 */
struct jmsg {
	struct json_easy easy;
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
 * Wait for a jmsg of a given type. Other messages are queued.
 * @param sig Signal backend
 * @param type Type of message to wait for (from JSON type field)
 * @return jmsg or NULL on error
 */
struct jmsg *jmsg_wait(struct cbot_signal_backend *sig, const char *type);

/**
 * Wait for a jmsg where @a field has value @a value
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

/**
 * Lookup @c key within the object at index @c n in the message @c jm.
 * @param jm JSON message
 * @param n The index of the object to look in
 * @param key The key to search - may be a JSON object expression.
 */
static inline int jmsg_lookup_at(struct jmsg *jm, uint32_t n, const char *key,
                                 uint32_t *res)
{
	return json_easy_lookup(&jm->easy, n, key, res);
}

/**
 * Lookup @c key in the top-level object of JSON message @c jm.
 * @param jm JSON message
 * @param key The key to search - may be a JSON object expression.
 */
static inline uint32_t jmsg_lookup(struct jmsg *jm, const char *key,
                                   uint32_t *res)
{
	return jmsg_lookup_at(jm, 0, key, res);
}

/**
 * Lookup @c key in an object it index @c n. Return it as a string.
 * @param jm JSON message
 * @param n The index of the object to look in
 * @param key The key to search - may be a JSON object expression.
 * @param[out] len Optional length of string
 */
char *jmsg_lookup_string_at_len(struct jmsg *jm, uint32_t n, const char *key,
                                size_t *len);

static inline char *jmsg_lookup_string_at(struct jmsg *jm, uint32_t n,
                                          const char *key)
{
	return jmsg_lookup_string_at_len(jm, n, key, NULL);
}

/**
 * Lookup @c key in the top-level object of JSON message @c jm.
 * @param jm JSON message
 * @param key The key to search - may be a JSON object expression.
 */
static inline char *jmsg_lookup_string(struct jmsg *jm, const char *key)
{
	return jmsg_lookup_string_at(jm, 0, key);
}

/***** some helpers related to JSON (move to nosj eventually) */

int je_get_object(struct json_easy *je, uint32_t start, const char *key,
                  uint32_t *out);
int je_get_array(struct json_easy *je, uint32_t start, const char *key,
                 uint32_t *out);
int je_get_uint(struct json_easy *je, uint32_t start, const char *key,
                uint64_t *out);
int je_get_int(struct json_easy *je, uint32_t start, const char *key,
               int64_t *out);
int je_get_bool(struct json_easy *je, uint32_t start, const char *key,
                bool *out);
int je_get_string(struct json_easy *je, uint32_t start, const char *key,
                  char **out);
bool je_string_match(struct json_easy *je, uint32_t start, const char *key,
                     const char *cmp);

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

/***** api.c *****/

struct signal_member {
	char *uuid;
	enum signal_role {
		SIGNAL_ROLE_DEFAULT,
		SIGNAL_ROLE_ADMINISTRATOR,
	} role;
};
struct signal_group {
	struct sc_list_head list;
	char *id;
	char *title;
	char *invite_link;
	struct signal_member *members;
	size_t n_members;
};

struct signal_user {
	struct sc_list_head list;
	char *first_name;
	char *last_name;
	char *number;
	char *uuid;
};

/**
 * Read another message from signald, expecting one of type @c type.
 * The resulting message is discarded, so the assumption is that you only care
 * that your message has been acknowledged successfully.
 * @param sig Signal backend
 * @param type The expected message type.
 */
void sig_expect(struct cbot_signal_backend *sig, const char *type);

/**
 * Wait for a message from signald whose ID matches the command we just sent
 * one, and expect it to have type @a type. On failure, return -1.
 */
int sig_result(struct cbot_signal_backend *sig, const char *type);
struct jmsg *sig_get_result(struct cbot_signal_backend *sig, const char *type);

/**
 * Get the profile of a user (by phone number).
 * @param sig Signal backend
 * @param phone Phone number of user
 * @return User object
 */
struct signal_user *sig_get_profile(struct cbot_signal_backend *sig,
                                    const char *phone);
/** Get the profile of a user (by uuid). */
struct signal_user *sig_get_profile_by_uuid(struct cbot_signal_backend *sig,
                                            const char *uuid);

/**
 * Call the list_contacts signald API.
 * It's not clear the exact semantics of this, but my expectation is that it
 * lists all users we have contacted via signal groups?
 * @param sig Signal backend
 * @param[out] list Linked list to attach all users to
 */
void sig_list_contacts(struct cbot_signal_backend *sig,
                       struct sc_list_head *list);
/** Free the user object */
void sig_user_free(struct signal_user *user);
/** Free all user objects in the list */
void sig_user_free_all(struct sc_list_head *list);

/** Lookup a user's number by UUID. */
char *sig_get_number(struct cbot_signal_backend *sig, const char *uuid);
/** Lookup a user's UUID by number. */
char *sig_get_uuid(struct cbot_signal_backend *sig, const char *number);

/**
 * List the groups that we are a member of.
 * @param sig Signal backend
 * @param list List to hook them onto
 */
int sig_list_groups(struct cbot_signal_backend *sig, struct sc_list_head *list);
/** Free a group object */
void sig_group_free(struct signal_group *grp);
/** Free all groups on the list */
void sig_group_free_all(struct sc_list_head *list);

/**
 * Subscribe to messages from Signald.
 * @param sig Signal backend
 */
int sig_subscribe(struct cbot_signal_backend *sig);

/**
 * Set our profile name.
 * @param sig Signal backend
 * @param name Name to set
 */
int sig_set_name(struct cbot_signal_backend *sig, const char *name);

#endif // CBOT_SIGNALD_INTERNAL_DOT_H
