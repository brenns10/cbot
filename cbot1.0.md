CBOT 1.0 Design
===============

This is a rough sketch of what I'd like to achieve for the 1.0 release of CBot.

Things that stay the same
-------------------------

* Plugins are shared objects which get dynamically loaded.
* Several "backends" exist, which actually drive the execution of the bot.
* Main bot interface from the backend is `cbot_handle_event()`.

Things that are different
-------------------------

* No more function pointer API - just create a 'libcbot.so' and link the main
  function to it, as well as the plugins.

* Responsibilities of the bot:

  - Allow plugins to subscribe to several event types:

    (regex event types)

    * channel message matching regex
    * channel message, addressed to bot, matching regex
    * action message, matching regex

    (passive event types)

    * channel message
    * action message

    (informative events)

    * user joins, parts, nick changes

  - Allow plugins to be un-registered!

  - Tracks users in each channel, allowing plugins to iterate over each user in
    the channel.

  - Provide plugins with a sqlite connection, and allow them to create tables if
    they do not exist during initialization, then use them to implement
    functionality.
