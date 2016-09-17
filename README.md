cbot
====

CBot is an IRC chatbot written entirely in C!

Features
--------

* Plugin based architecture: all bot features are implemented as dynamically
  loaded plugins.
* Plugins handle events that they match based on regular expression (which is my
  own regex implementation!)
* Plugins can perform the following actions:
  * Send to a channel or user.
  * Send an "action" message (`/me does something`)
* Plugins can handle the following events:
  * Normal channel message.
  * Action message in a channel.
  * User joining and leaving a channel.

Future Work
-----------

* Handle/send invitations to channels.
* Handle kick messages.  Maybe kick people?
* Be able to send messages.
* Be able to join/leave channels.
* Provide list of users in a channel.
* Allow plugins to keep state.

Plugin List
-----------

- name: Responds to questions about CBot with a link to the github.
- help: An outdated command listing.
- karma: Tracks the karma of various words.
- greet: Say hi back to people, as well as greet when they enter a channel, and
  say bad things when they leave.
- sadness: Responds to some forms of insult with odd comebacks.
- emote: CBot will echo what you said in an ACTION (`/me`) message.

Building and Running
--------------------

Dependencies:
- `libircclient`: On Arch Linux, the package is `libircclient`. For Ubuntu, I
  would guess you need to run `sudo apt-get install libircclient-devel`, but I
  haven't tried it myself. You can also statically link to libircclient, by
  simply passing `LIBIRCCLIENT_LOCAL=yes` as an argument to `make`, which will
  download, build, and link libircclient into CBot correctly.
- `libstephen`: Simply run `git submodule init` and `git submodule update` in
  the repository root. Any time the `libstephen` version is updated, you'll
  probably need to run `git submodule update` again.

Compiling: just run `make`.  Running: run `bin/release/cbot` with the following arguments:

1. Either `cli` or `irc`, depending on whether you want to run locally or on a
   real IRC server.
2. If you chose the `irc` backend, you must specify:
   - `--host [hostname]` - the IRC server host
   - `--port [port]` - the IRC server port
   - `--chan [chan]` - channel to join on startup
3. Regardless of your backend, you can specify:
   - `--name [name]` - a name for the bot (other than cbot)
   - `--plugin-dir [dir]` - tell the program where your plugins reside
4. Finally, provide a list of plugins to load. See the list above. Note that you
   should provide the names without the `.so` extension.

Plugin API
----------

Here's a brief rundown of how to write a plugin:

- Create `[somename].c`.
- Inside, write a function `void [somename]_load(cbot_t *bot, cbot_register_t
  registrar)`. For now, leave it blank. Make sure to also `#include
  "cbot/cbot.h"` at the top.
- Now, create a handler function. This is the heart of a plugin. It handles an
  *event* by taking some *action* on it. So, it will have the following
  signature: `void give_me_a_name(cbot_event_t event, cbot_actions_t actions)`.
- The event argument is a struct with:
  - `const cbot_t* bot` - a pointer to the bot
  - `cbot_event_type_t type` - the type of event (described later)
  - `const char *channel` - the channel name associated with the event
  - `const char *username` - the username associated with the event
- The action argument has two functions in it:
  - `send`, which has signature `void send(const cbot_t *bot, const char *dest,
    const char *format, ...)`. It is like `printf` in that it accepts a format
    string and a variable number of arguments. It can send to a channel or a
    user.
  - `me`, which has the same signature as `send`. Its only difference is that it
    sends an "action" or `/me` message.
- After you've written your handler function (or at least declared it), go back
  to your `[somename]_load` function. Put the following function call in it:
  
      ```
      registrar(bot, CBOT_CHANNEL_MSG, give_me_a_name);
      ```
  
- The argument `CBOT_CHANNEL_MSG` tells cbot that this handles a message in a
  channel. You can handle the following additional events:
  - `CBOT_CHANNEL_ACTION`: an action message in a channel
  - `CBOT_JOIN`: a person joining a channel (could be CBot itself)!
  - `CBOT_PART`: a person leaving a channel
- The `give_me_a_name` function will be called for every channel message.
- Put the C file in the `plugins/` directory, and use the makefile to build!
  Tada!

License
-------

This project is under the Revised BSD license.  Please see
[`LICENSE.txt`](LICENSE.txt) for details.
