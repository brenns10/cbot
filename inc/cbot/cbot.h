/***************************************************************************//**

  @file         cbot.h

  @author       Stephen Brennan

  @date         Created Wednesday, 22 July 2015

  @brief        Declarations for CBot!

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#ifndef CBOT_H
#define CBOT_H

#include <stddef.h>

struct cbot;
typedef struct cbot cbot_t;

/**
   An enumeration of possible events that can be handled by a plugin.
 */
typedef enum {

  /* CBot hears a message in a channel (not addressed to it) */
  CBOT_CHANNEL_HEAR,
  /* CBot has a message in a channel addressed to it. */
  CBOT_CHANNEL_MSG,

} cbot_event_type_t;

/**
   Details associated with an event, which are passed to an event handler.
 */
typedef struct {

  const cbot_t *bot;

  cbot_event_type_t type;
  const char *channel;
  const char *username;
  const char *message;

  size_t num_captures;
  const size_t *capture_starts;
  const size_t *capture_ends;

} cbot_event_t;

/**
   A struct filled with actions that can be taken on a bot.
 */
typedef struct {

  void (*send)(const cbot_t *bot, const char *dest, const char *format, ...);
  void (*me)(const cbot_t *bot, const char *dest, const char *format, ...);

} cbot_actions_t;

/**
   An event handler function. Takes an event and does some action to handle it.

   This is the meat of the plugin ecosystem. The main job of plugins is to write
   handlers. These handlers take event parameters, a structure filled with
   actions, and then they do something.

   @param event Structure containing details of the event to handle.
   @param actions Structure containing action functions available.
 */
typedef void (*cbot_handler_t)(cbot_event_t event,
                               cbot_actions_t actions);

/**
   An event registrar function.

   This function is passed to plugins on load, so that they may register all
   their event handlers.

   @param bot The bot you are registering with.
   @param event Event you are registering to handle.
   @param regex Regex to match on the event text (mostly for messages).
   @param handler Event handler function.
 */
typedef void (*cbot_register_t)(cbot_t *bot, cbot_event_type_t event,
                                const char *regex,
                                cbot_handler_t handler);


/**
   Main plugin loader function.

   All plugins must have a plugin loader. If they are named "plugin.c", then
   they should compile to "plugin.so", and their loader should be named
   "plugin_load", and it should have this exact signature. This function
   receives a bot instance and a registrar function. With that, it registers all
   its event handlers accordingly.

   @param bot CBot instance for the plugin.
   @param registrar Function to call to register each plugin.
 */
typedef void (*cbot_plugin_t)(cbot_t *bot, cbot_register_t registrar);

#endif//CBOT_H
