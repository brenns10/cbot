#ifndef CBOT_SIGNAL_INTERNAL_DOT_H
#define CBOT_SIGNAL_INTERNAL_DOT_H

#include <sys/types.h>
#include <stdio.h>

#include <nosj.h>

struct cbot_signal_backend {

	/* The Unix domain socket connecting us to Signald */
	int fd;

	/* Any data from the last read which hasn't yet been processed */
	char *spill;
	size_t spilllen;

	/*
	 * A stdio write stream associated with the above socket. It is in
	 * unbuffered mode, used to write formatted JSON commands.
	 */
	FILE *ws;

	/* Phone number of the bot sender */
	char *sender;

	/* Reference to the bot */
	struct cbot *bot;
};

/***** jmsg.c *****/

/*
 * Structure representing a line of text which is a JSON message.
 * Owns the orig and tok pointers (though tok may be null).
 * Can be parsed, and then subsequent lookup operations can happen.
 */
struct jmsg {
	char *orig;
	size_t origlen;

	struct json_token *tok;
	size_t toklen;
};

/**
 * Read a JSON message from Signald.
 *
 * This may yield to the event loop waiting for input. It reads until a full
 * line is received, and buffers any remaining data transparently.
 *
 * @param sig Signal backend
 * @return NULL on error, otherwise a struct jmsg ready to parse
 */
struct jmsg *jmsg_read(struct cbot_signal_backend *sig);

/**
 * Parse a JSON message.
 * @param sig Signal backend
 * @return -1 on error, otherwise 0 and the message is ready for use
 */
int jmsg_parse(struct jmsg *jm);

/**
 * Combine the read and parse steps together into one.
 * @param sig Signal backend
 * @return NULL on error, otherwise a struct jmsg ready to use
 */
struct jmsg *jmsg_read_parse(struct cbot_signal_backend *sig);

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
static inline size_t jmsg_lookup_at(struct jmsg *jm, size_t n, const char *key)
{
	return json_lookup(jm->orig, jm->tok, n, key);
}

/**
 * Lookup @c key in the top-level object of JSON message @c jm.
 * @param jm JSON message
 * @param key The key to search - may be a JSON object expression.
 */
static inline size_t jmsg_lookup(struct jmsg *jm, const char *key)
{
	return jmsg_lookup_at(jm, 0, key);
}

/**
 * Lookup @c key in an object it index @c n. Return it as a string.
 * @param jm JSON message
 * @param n The index of the object to look in
 * @param key The key to search - may be a JSON object expression.
 */
char *jmsg_lookup_string_at(struct jmsg *jm, size_t n, const char *key);

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
 * Formats a mention placeholder.
 * @param string The value of the mention
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
char *mention_from_json(const char *str, struct jmsg *jm, size_t list);

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

/**
 * Read another message from signald, expecting one of type @c type.
 * The resulting message is discarded, so the assumption is that you only care
 * that your message has been acknowledged successfully.
 * @param sig Signal backend
 * @param type The expected message type.
 */
void sig_expect(struct cbot_signal_backend *sig, const char *type);

/**
 * Get the profile of a user (by phone number).
 *
 * Currently, this just prints it out. Future plans to make this into a real
 * API.
 *
 * @param sig Signal backend
 * @param phone Phone number of user
 */
void sig_get_profile(struct cbot_signal_backend *sig, char *phone);

/**
 * List the groups that we are a member of.
 *
 * Currently, this just prints. Future plans to make this into a real API.
 *
 * @param sig Signal backend
 */
void sig_list_groups(struct cbot_signal_backend *sig);

/**
 * Subscribe to messages from Signald.
 * @param sig Signal backend
 */
void sig_subscribe(struct cbot_signal_backend *sig);

/**
 * Set our profile name.
 * @param sig Signal backend
 * @param name Name to set
 */
void sig_set_name(struct cbot_signal_backend *sig, char *name);

/**
 * Send a message to a group.
 * @param sig Signal backend
 * @param groupId Group ID to send to
 * @param msg Message (may contain mentioned usernames)
 */
void sig_send_group(struct cbot_signal_backend *sig, const char *groupId, const char *msg);

/**
 * Send a message to a single user recipient.
 * @param sig Signal backend
 * @param uuid UUID of the user to send to
 * @param msg Message (may contain mentioned usernames)
 */
void sig_send_single(struct cbot_signal_backend *sig, const char *uuid, const char *msg);

#endif // CBOT_SIGNAL_INTERNAL_DOT_H
