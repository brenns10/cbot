#ifndef CBOT_SIGNAL_INTERNAL_DOT_H
#define CBOT_SIGNAL_INTERNAL_DOT_H

#include <stdio.h>
#include <sys/types.h>

#include <cbot/cbot.h>
#include <nosj.h>
#include <sc-collections.h>
#include <sc-lwt.h>

struct signal_user;

struct signal_reaction_cb {
	/* Timestamp of the message which we sent, that may get reacted */
	uint64_t ts;
	/* Operations from the plugin */
	struct cbot_reaction_ops ops;
	/* Argument to plugin */
	void *arg;
};

struct signal_queued_item {
	struct sc_list_head list;
	const char *field;
	const char *value;
	struct sc_lwt *thread;
	struct jmsg *result;
};

struct cbot_signal_backend {

	/* The Unix domain socket connecting us to Signald */
	int fd;

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

	/* Phone number of the bot sender */
	char *sender;
	struct signal_user *bot_profile;

	/* Phone number of authorized user */
	char *auth;
	struct signal_user *auth_profile;

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
char *mention_format_p(char *string, const char *prefix);

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
 * @param jm The JSON message buffer
 * @param list The index of the array to read menitons from
 * @return A newly allocated string with mentions expanded to placeholders.
 */
char *mention_from_json(const char *str, struct jmsg *jm, uint32_t list);

/**
 * Return a newly allocated string with necessary escaping for JSON. Return a
 * second string (in mentions) which contains all mention JSON elements.
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
 * @param[out] mentions Will be populated with a newly allocated string of JSON
 * mentions as an array.
 */
char *json_quote_and_mention(const char *instr, char **mentions);

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

/**
 * Send a message to a group.
 * @param sig Signal backend
 * @param groupId Group ID to send to
 * @param msg Message (may contain mentioned usernames)
 */
uint64_t sig_send_group(struct cbot_signal_backend *sig, const char *groupId,
                        const char *msg);

/**
 * Send a message to a single user recipient.
 * @param sig Signal backend
 * @param uuid UUID of the user to send to
 * @param msg Message (may contain mentioned usernames)
 */
uint64_t sig_send_single(struct cbot_signal_backend *sig, const char *uuid,
                         const char *msg);

bool sig_reaction_cb(struct cbot_signal_backend *sig, uint64_t ts,
                     struct signal_reaction_cb *out);
#endif // CBOT_SIGNAL_INTERNAL_DOT_H
