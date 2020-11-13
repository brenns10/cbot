cbot
====

CBot is an IRC chatbot written entirely in C!

Why would you do that? Because I can.

Features
--------

* Plugin based architecture: all bot features are implemented as dynamically
  loaded plugins.
* Plugins can handle events such as:
  - channel & direct messages (filtered by regex)
  - user joins / leaves, and nickname changes
* Plugins can respond to events by:
  - sending channel and direct messages
  - joining channels
  - setting user "op" mode
  - changing bot nickname
* The bot and plugins can be configured via a
  [libconfig](http://hyperrealm.github.io/libconfig/) file, allowing for lots of
  flexibility.
* Plugins can store data in a persistent sqlite database. CBot comes with a
  straightforward schema migration system, and a set of macros which can help
  write query functions.
* The entire bot framework is based on a lightweight threading system and event
  loop. This allows asynchronous I/O code, such as HTTP requests, to
  cooperatively multitask with the IRC loop without launching true OS threads
  and dealing with concurrency.
* Several small utility APIs exist to assist in writing plugins:
  - Dynamic arrays, hash table, linked list, string builder via `sc-collections`
  - Tokenizing API for turning messages into command arguments via a simple
    quoting system
  - Argument parsing API via `sc-argparse`, in case you want to go full UNIX
  - String templating/formatting API based on callbacks

Plugin List
-----------

- name: Responds to questions about CBot with a link to the github.
- help: An outdated command listing.
- [sql]karma: Tracks the karma of various words. The SQL variation uses
  persistent storage
- know: You can tell cbot to "know" what things are and ask it later. EG "cbot
  know that Taylor Swift is awesome", and then "cbot what is Taylor Swift?"
- greet: Say hi back to people, as well as greet when they enter a channel, and
  say bad things when they leave.
- sadness: Responds to some forms of insult with odd comebacks.
- emote: CBot will echo what you said in an ACTION (`/me`) message.
- sqlknow: Stores knowledge about various terms: `know that X is Y`, and
  regurgitates this knowledge on command: `what is X?`
- become: users can change the bot nickname...
- who: lists out the users in the channel, beware
- tok: a simple demo of "command tokenizing"
- log: message logging
- reply: a configurable plugin for triggering responses to messages matching
  regex

Building
--------

This project uses the Meson build system. It also depends on libconfig and
openssl, which need to be installed on your system. On Ubuntu, the proper
minimal packages would probably be `meson libconfig-dev libssl-dev`.  There are
additional dependencies which can be compiled if not installed, see below. The
following commands will build the project:

    meson build
    ninja -C build

The first command creates a build directory, and the second command runs the
"ninja" build system (Meson's backend) in the build directory. Compiled output
is placed into the build directory (both the `cbot` binary and all plugins).

Meson will automatically download and build the following dependencies, if they
are not installed on your system:

- `libircclient` (Ubuntu package is `libircclient-dev`)
- `sc-regex`, `sc-collections`, `sc-argparse`, `sc-lwt`: no distribution
  packages, these must be built from source
- `sqlite3` (Ubuntu package in `libsqlite3-dev`)

Running
-------

`build/cbot` takes a single argument, a configuration file. See `sample.cfg` for
a sample which you can modify for IRC connections or CLI experimentation.

Plugin API
----------

See [doc/Plugins.md](doc/Plugins.md) for details.

License
-------

This project is under the Revised BSD license.  Please see
[`LICENSE.txt`](LICENSE.txt) for details.
