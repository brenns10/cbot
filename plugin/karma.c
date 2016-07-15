/***************************************************************************//**

  @file         karma.c

  @author       Stephen Brennan

  @date         Created Thursday, 30 July 2015

  @brief        CBot "echo" handler to demonstrate regex capture support!

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libstephen/re.h"
#include "cbot/cbot.h"

/**
   A type representing a single karma entry, which is a word, number pair.
 */
typedef struct {
  int karma;
  char *word;
} karma_t;

/**
   Pointer to the karma array.
 */
static karma_t *karma = NULL;
/**
   Number of karma allocated.
 */
static size_t karma_alloc = 128;
/**
   Number of karma in the array.
 */
static size_t nkarma = 0;

/**
   A regex which matches karma increments and decrements.
   - capture 0: the word
   - capture 1: the operation (++ or --)
 */
static Regex augment;
/**
   A regex which matches the karma command, including its optional argument.
   - capture 0: space plus capture 1
   - capture 1: the word being "asked" about, or empty string
 */
static Regex check;

/**
   @brief Return the index of a word in the karma array, or -1.
   @param word The word to look up.
   @returns The word's index, or -1 if it is not present in the array.
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
   @brief Copy a string.
   @param word The word to copy.
   @returns A newly allocated copy of the string.
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
   @brief Return the index of any word in the karma array.

   This function will locate an existing word in the karma array and return its
   index. If the word doesn't exist, it will copy it, add it to the karma array,
   set its karma to zero, and return its index.

   @param word Word to find karma of.  Never modified.
   @returns Index of the word in the karma array.
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
    karma[idx] = (karma_t) {.word=copy_string(word), .karma=0};
  }
  return idx;
}

/*
  These functions are for sorting karma entries using C's built in qsort!
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
   How many words to print karma of?
 */
#define KARMA_TOP 5

/**
   @brief Print the top KARMA_BEST words.
   @param event The event we're responding to.
   @param actions Actions available to us.
 */
static void karma_best(cbot_event_t event, cbot_actions_t actions)
{
  size_t i;
  karma_sort();
  for (i = 0; i < (nkarma > KARMA_TOP ? KARMA_TOP : nkarma); i++) {
    actions.send(event.bot, event.channel, "%d. %s (%d karma)", i+1,
                 karma[i].word, karma[i].karma);
  }
}

/**
   @brief Lookup a word's karma and send it in a message to the event origin.
   @param word The word to look up karma for.
   @param event The event we're responding to.
   @param actions Actions given to us by bot.
 */
static void karma_check(const char *word, cbot_event_t event, cbot_actions_t actions)
{
  ssize_t index;

  // An empty capture means we should list out the best karma.
  if (strcmp(word, "") == 0) {
    karma_best(event, actions);
    return;
  }

  index = find_karma(word);
  if (index < 0) {
    actions.send(event.bot, event.channel, "%s has no karma yet", word);
  } else {
    actions.send(event.bot, event.channel, "%s has %d karma", word, karma[index].karma);
  }
}

/**
   @brief The karma plugin's single handler.

   This function receives an IRC message event and handles the "check",
   "increment", and "decrement" operations that are available.

   @param event Information for the event.
   @param actions Actions the plugin may take.
 */
static void karma_handler(cbot_event_t event, cbot_actions_t actions)
{
  size_t *rawcap;
  Captures captures;
  int increment = actions.addressed(event.bot, event.message);

  if (increment && reexec(check, event.message + increment, &rawcap) != -1) {
    // When the message is addressed to the bot and matches the karma check
    // regular expression.

    // Execute the karma check operation, using the second regex capture.
    captures = recap(event.message + increment, rawcap, renumsaves(check));
    karma_check(captures.cap[1], event, actions);
    recapfree(captures);
  } else if (reexec(augment, event.message, &rawcap) != -1) {
    // Otherwise, when the message matches the "augment" regular expression, we
    // update the karma.
    captures = recap(event.message, rawcap, renumsaves(check));
    increment = strcmp(captures.cap[1], "++") == 0 ? 1 : -1;
    karma_change(captures.cap[0], increment);
    recapfree(captures);
  }
}

/**
   @brief Plugin loader function.

   Compiles necessary regular expressions and registers a single handle function
   to handle any incoming channel messages.

   @param bot Bot we're loading into.
   @param registrar Function to call to register handlers.
 */
void karma_load(cbot_t *bot, cbot_register_t registrar)
{
  #define KARMA_WORD "^ \t\n"
  #define NOT_KARMA_WORD " \t\n"
  augment = recomp(".*?([" KARMA_WORD "]+)(\\+\\+|--).*?");
  check = recomp("karma(\\s+([" KARMA_WORD "]+))?");
  registrar(bot, CBOT_CHANNEL_MSG, karma_handler);
}
