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
 * A regex which matches karma increments and decrements.
 * - capture 0: the word
 * - capture 1: the operation (++ or --)
 */
static struct sc_regex *augment;
/**
 * A regex which matches the karma command, including its optional argument.
 * - capture 0: space plus capture 1
 * - capture 1: the word being "asked" about, or empty string
 */
static struct sc_regex *check;
/**
 * A regex for admin set command
 * - capture 0: hash
 * - capture 1: the word
 * - capture 2: the number
 */
static struct sc_regex *set;
/**
 *A regex for forgetting a person
 */
static struct sc_regex *forget;

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

static void karma_change(const char *word, int change)
{
	size_t index;
	index = find_or_create_karma(word);
	karma[index].karma += change;
}

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
 * @param actions Actions available to us.
 */
static void karma_best(struct cbot_event event, struct cbot_actions actions)
{
	size_t i;
	karma_sort();
	for (i = 0; i < (nkarma > KARMA_TOP ? KARMA_TOP : nkarma); i++) {
		actions.send(event.bot, event.channel, "%d. %s (%d karma)",
		             i + 1, karma[i].word, karma[i].karma);
	}
}

/**
 * @brief Lookup a word's karma and send it in a message to the event origin.
 * @param word The word to look up karma for.
 * @param event The event we're responding to.
 * @param actions Actions given to us by bot.
 */
static void karma_check(const char *word, struct cbot_event event,
                        struct cbot_actions actions)
{
	ssize_t index;

	// An empty capture means we should list out the best karma.
	if (strcmp(word, "") == 0) {
		karma_best(event, actions);
		return;
	}

	index = find_karma(word);
	if (index < 0) {
		actions.send(event.bot, event.channel, "%s has no karma yet",
		             word);
	} else {
		actions.send(event.bot, event.channel, "%s has %d karma", word,
		             karma[index].karma);
	}
}

/**
 * @brief The karma plugin's single handler.
 *
 * This function receives an IRC message event and handles the "check",
 * "increment", and "decrement" operations that are available.
 *
 * @param event Information for the event.
 * @param actions Actions the plugin may take.
 */
static void karma_handler(struct cbot_event event, struct cbot_actions actions)
{
	size_t *indices;
	int increment = actions.addressed(event.bot, event.message);

	if (increment &&
	    sc_regex_exec(check, event.message + increment, &indices) != -1) {
		// When the message is addressed to the bot and matches the
		// karma check regular expression.

		// Execute the karma check operation, using the second regex
		// capture.
		char *word = sc_regex_get_capture(event.message + increment,
		                                  indices, 1);
		karma_check(word, event, actions);
		free(word);
		free(indices);
	} else if (sc_regex_exec(augment, event.message, &indices) != -1) {
		// Or, when the message matches the "augment" regular
		// expression, we update the karma.
		char *word, *op;
		word = sc_regex_get_capture(event.message, indices, 0);
		op = sc_regex_get_capture(event.message, indices, 1);
		karma_change(word, strcmp(op, "++") == 0 ? 1 : -1);
		free(word);
		free(op);
		free(indices);
	} else if (increment && sc_regex_exec(set, event.message + increment,
	                                      &indices) != -1) {
		// Or, when the message is addressed to the bot and the command
		// is to set-karma, we will attempt to auth the command and
		// execute it.
		char *hash, *word, *number;
		hash = sc_regex_get_capture(event.message + increment, indices,
		                            2);
		word = sc_regex_get_capture(event.message + increment, indices,
		                            0);
		number = sc_regex_get_capture(event.message + increment,
		                              indices, 1);
		if (actions.is_authorized(event.bot, hash)) {
			int index = find_or_create_karma(word);
			karma[index].karma = atoi(number);
		} else {
			actions.send(
			        event.bot, event.channel,
			        "sorry, you're not authorized to do that!");
		}
		free(word);
		free(hash);
		free(number);
		free(indices);
	} else if (increment && sc_regex_exec(forget, event.message + increment,
	                                      NULL) != -1) {
		delete_if_exists(event.username);
	}
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
void karma_load(struct cbot *bot, cbot_register_t registrar)
{
#define KARMA_WORD "^ \t\n"
#define NOT_KARMA_WORD " \t\n"
	augment = sc_regex_compile(".*?([" KARMA_WORD "]+)(\\+\\+|--).*?");
	check = sc_regex_compile("karma(\\s+([" KARMA_WORD "]+))?");
	set = sc_regex_compile("set-karma +([" KARMA_WORD
	                       "]+) +(-?\\d+) +([A-Za-z0-9+/=]+)");
	forget = sc_regex_compile("forget-me");
	registrar(bot, CBOT_CHANNEL_MSG, karma_handler);
}
