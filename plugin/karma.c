/**
 * karma.c: CBot plugin which tracks karma (++ and --)
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "sc-regex.h"

#include "cbot/cbot.h"

/**
 * A type representing a single karma entry, which is a word, number pair.
 */
typedef struct {
	int karma;
	char *word;
} karma_t;

/**
 * Pointer to the karma array.
 */
static karma_t *karma = NULL;
/**
 * Number of karma allocated.
 */
static size_t karma_alloc = 128;
/**
 * Number of karma in the array.
 */
static size_t nkarma = 0;

/**
 * @brief Return the index of a word in the karma array, or -1.
 * @param word The word to look up.
 * @returns The word's index, or -1 if it is not present in the array.
 */
static ssize_t find_karma(const char *word)
{
	size_t i;
	for (i = 0; i < nkarma; i++) {
		if (strcmp(karma[i].word, word) == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * @brief Copy a string.
 * @param word The word to copy.
 * @returns A newly allocated copy of the string.
 */
static char *copy_string(const char *word)
{
	char *newword;
	size_t length = strlen(word);
	newword = malloc(length + 1);
	strncpy(newword, word, length + 1);
	newword[length] = '\0';
	return newword;
}

/**
 * @brief Return the index of any word in the karma array.
 *
 * This function will locate an existing word in the karma array and return its
 * index. If the word doesn't exist, it will copy it, add it to the karma array,
 * set its karma to zero, and return its index.
 *
 * @param word Word to find karma of.  Never modified.
 * @returns Index of the word in the karma array.
 */
static size_t find_or_create_karma(const char *word)
{
	ssize_t idx;
	if (karma == NULL) {
		karma = calloc(karma_alloc, sizeof(karma_t));
	}
	idx = find_karma(word);
	if (idx < 0) {
		if (nkarma == karma_alloc) {
			karma_alloc *= 2;
			karma = realloc(karma, karma_alloc * sizeof(karma_t));
		}
		idx = nkarma++;
		karma[idx] = (karma_t){ .word = copy_string(word), .karma = 0 };
	}
	return idx;
}

/**
 * @brief Removes a word from the karma array if it exists
 *
 * @param word Word to find and delete
 * @returns 1 if deleted, zero if not
 */
static size_t delete_if_exists(const char *word)
{
	ssize_t idx;
	if (karma == NULL) {
		karma = calloc(karma_alloc, sizeof(karma_t));
	}
	idx = find_karma(word);
	if (idx >= 0) {
		free(karma[idx].word);
		karma[idx] = karma[--nkarma];
		return 1;
	}
	return 0;
}

/*
 *These functions are for sorting karma entries using C's built in qsort!
 */

static int karma_compare(const void *l, const void *r)
{
	const karma_t *lhs = l, *rhs = r;
	return rhs->karma - lhs->karma;
}

static void karma_sort()
{
	qsort(karma, nkarma, sizeof(karma_t), karma_compare);
}

/**
 * How many words to print karma of?
 */
#define KARMA_TOP 5

/**
 * @brief Print the top KARMA_BEST words.
 * @param event The event we're responding to.
 */
static void karma_best(struct cbot_message_event *event)
{
	size_t i;
	karma_sort();
	for (i = 0; i < (nkarma > KARMA_TOP ? KARMA_TOP : nkarma); i++) {
		cbot_send(event->bot, event->channel, "%d. %s (%d karma)",
		          i + 1, karma[i].word, karma[i].karma);
	}
}

static void karma_check(struct cbot_message_event *event, void *user)
{
	ssize_t index;
	char *word = sc_regex_get_capture(event->message, event->indices, 1);

	// An empty capture means we should list out the best karma.
	if (strcmp(word, "") == 0) {
		free(word);
		karma_best(event);
		return;
	}

	index = find_karma(word);
	if (index < 0) {
		cbot_send(event->bot, event->channel, "%s has no karma yet",
		          word);
	} else {
		cbot_send(event->bot, event->channel, "%s has %d karma", word,
		          karma[index].karma);
	}
	free(word);
}

static void karma_change(struct cbot_message_event *event, void *user)
{
	char *word = sc_regex_get_capture(event->message, event->indices, 0);
	char *op = sc_regex_get_capture(event->message, event->indices, 1);
	int index = find_or_create_karma(word);
	karma[index].karma += (strcmp(op, "++") == 0 ? 1 : -1);
	free(word);
	free(op);
}

static void karma_set(struct cbot_message_event *event, void *user)
{
	char *word, *value, *hash;
	int index;
	hash = sc_regex_get_capture(event->message, event->indices, 2);
	if (!cbot_is_authorized(event->bot, hash)) {
		free(hash);
		cbot_send(event->bot, event->channel,
		          "sorry, you're not authorized to do that!");
		return;
	}

	word = sc_regex_get_capture(event->message, event->indices, 0);
	value = sc_regex_get_capture(event->message, event->indices, 1);
	index = find_or_create_karma(word);
	karma[index].karma = atoi(value);
	free(word);
	free(value);
	free(hash);
}

static void karma_forget(struct cbot_message_event *event, void *user)
{
	delete_if_exists(event->username);
}

/**
 * @brief Plugin loader function.
 *
 * Compiles necessary regular expressions and registers a single handle function
 * to handle any incoming channel messages.
 *
 * @param bot Bot we're loading into.
 * @param registrar Function to call to register handlers.
 */
void karma_load(struct cbot *bot)
{
#define KARMA_WORD "^ \t\n"
#define NOT_KARMA_WORD " \t\n"
	cbot_register(bot, CBOT_ADDRESSED, (cbot_handler_t)karma_check, NULL,
	              "karma(\\s+([" KARMA_WORD "]+))?");
	cbot_register(bot, CBOT_MESSAGE, (cbot_handler_t)karma_change, NULL,
	              ".*?([" KARMA_WORD "]+)(\\+\\+|--).*?");
	cbot_register(bot, CBOT_ADDRESSED, (cbot_handler_t)karma_set, NULL,
	              "set-karma +([" KARMA_WORD
	              "]+) +(-?\\d+) +([A-Za-z0-9+/=]+)");
	cbot_register(bot, CBOT_ADDRESSED, (cbot_handler_t)karma_forget, NULL,
	              "forget[ -]me");
}
