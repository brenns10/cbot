cbot
====

[![builds.sr.ht status](https://builds.sr.ht/~brenns10/cbot/commits.svg)](https://builds.sr.ht/~brenns10/cbot/commits?)

CBot is a chatbot written entirely in C!

Why would you do that? Because I can.


Support
-------

CBot can be used with the following chat systems:

1. IRC
2. Signal (thanks to the Signald API bridge)
3. Your terminal (this one is pretty lonely)


Abilities
---------

These are some more interesting ones.

- karma: Tracks the karma of various words. Uses persistent storage so you'll
  never forget all those times you said "cbot--"
- know: You can tell cbot to "know" what things are and ask it later. EG "cbot
  know that Taylor Swift is awesome", and then "cbot what is Taylor Swift?"
- weather: Request weather at any zip code
- reply: a configurable plugin for triggering responses to messages matching
  regex. Think of this as Slackbot responses but much better.
- name: Responds to questions about CBot with a link to the github.
- greet: Say hi back to people, as well as greet when they enter a channel, and
  say bad things when they leave. (IRC only)
- sadness: Responds to some forms of insult with odd comebacks.


Features
--------

* Both the chat backend (e.g. IRC or Signal) and the bot abilities (i.e.
  plugins) are modular. So it's easy (ish) to port CBot to Slack (pull requests
  welcome) or add a plugin that works on all of the above.
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


Build & Run
-----------

See [doc/admin.rst#installing-from-source](doc/admin.rst#installing-from-source) for details on build / install.

Plugin API
----------

See [doc/Plugins.md](doc/Plugins.md) for a details on plugin development.

License
-------

This project is under the Revised BSD license.  Please see
[`LICENSE.txt`](LICENSE.txt) for details.
