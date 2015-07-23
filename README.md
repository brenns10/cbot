cbot
====

CBot is an IRC chatbot written entirely in C!


Motivation
----------

On the [CWRU IRC Server](http://irc.case.edu), we have a silly little chatbot
named [Lulu](https://github.com/cwruacm/lulu).  Lulu is our own, customized
version of [Hubot](https://hubot.github.com/), GitHub's chatbot.  Unfortunately,
Hubot is written in in Coffeescript.  I don't like Coffeescript (for reasons
that aren't really relevent here), and figured that it would be really fun to
implement my own chatbot in a language I do like (Python and C being the two big
choices).  Looking for a challenge, I went with C.


Plan
----

CBot will be an easily extensible chatbot that duplicates some of Hubot's basic
functionality.  You will be able to implement CBot extensions by writing them in
C and creating a shared object file out of them.  Then, you will be able to load
them by providing CBot with the plugin filenames at runtime.

CBot will have an IRC backend as well as a console backend in order to allow
easier testing.


Current State
-------------

The following functionality is currently implemented:

* Plugin handler types:
    * Responding to any channel message.
* Allowing custom plugins via simple API.
* Dynamically loading plugins.
* An IRC backend that can connect to any server and channel.

Remaining to be implemented:

* Plugin handler types:
    * Responding to messages directed towards CBot (e.g. "cbot help" or "cbot:
      help");
    * Responding to a user entering the channel.
    * Responding to a user leaving the channel.
    * Responding to invitations.
* IRC actions:
    * Sending invitations
    * Emoting (e.g. "cbot cries")
    * Getting names of people in the room
* Console backend for simpler debugging.
* A core set of plugins.


Building and Running
--------------------

The only part of CBot that I don't implement myself is the IRC protocol - that
complexity would be unjustified.  Instead, I use the `libircclient` library,
which has proved to be excellent so far.  On Arch Linux, the package is
`libircclient`.  For Ubuntu, I would guess you need to run `sudo apt-get install
libircclient-devel`, but I haven't tried it myself.

In addition, you'll need to get my
[`libstephen`](https://github.com/brenns10/libstephen) library, which provides
the fundamental data structures and regular expression implementation that I use
in this project.  In order to do so, run `git submodule init` and `git submodule
update` in the repository root.  Any time the `libstephen` version is updated,
you'll probably need to run `git submodule update` again.

Finally, you should simply need to run `make` to build CBot.  The compiled
binary will be under `bin/release/cbot`.  You'll need to run `bin/release/cbot
irc` to activate the IRC backend.  Check `bin/release/cbot irc --help` for
argument information.

Plugins are specified on the command line.  You'll need to compile plugins in
the `plugin` directory - just change directory into there and run `make`.
Then, you can specify plugins by name on the command line.

So, to just run CBot on CWRU's IRC with the greeting plugin, you should do this:

    bin/release/cbot irc greeting


Contributing Plugins
--------------------

CBot is nearly at the point where writing plugins is possible.  See the
`plugin/` directory for example plugins to get you started.  The rules are:

* The plugin name should be a valid C identifier.  The code should be
  `plugin_name.c`, and it will be compiled to `plugin_name.so`.
* The plugin should define only one public symbol - `plugin_name_load()`.  This
  should be a function of type `cbot_plugin_t` (see
  [`inc/cbot/cbot.h`](inc/cbot/cbot.h)).  It must always be the plugin name,
  followed by `_load`.
* The load function will use its arguments (the `cbot_t` pointer and the
  function pointers) to register all its callbacks.
* The regular expression syntax is from libstephen.  It supports basic suffixes
  like `*`, `?`, and `+`, character classes, subexpressions via `()`, OR via
  `|`, and some built in character classes like `\w`, `\d`.  There is no
  capturing, which will probably hamper regular expression matching.  Do not use
  `^` or `$` to indicate line beginnings or endings.
* The regular expressions must match the *whole message*, not just a part of it.

If you've written a plugin you think is cool, you can submit a PR and I'll check
it out.


License
-------

This project is under the Revised BSD license.  Please see
[`LICENSE.txt`](LICENSE.txt) for details.
