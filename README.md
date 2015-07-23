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

CBot's very core functionality (responding to messages based on regular
expressions) is implemented, and a preliminary IRC backend is also implemented,
along with a single plugin that responds to greetings.  Right now you can run
CBot, and it will successfully connect to IRC and begin responding to greetings.


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
binary will be under `bin/release/cbot`.  All of the IRC server settings are
currently hardcoded in.  This will obviously change in the future, but if you'd
like to change anything, look for the definitions in
[`src/cbot_irc.c`](src/cbot_irc.c).


Contributing Plugins
--------------------

Right now, CBot is a bit too young to receive contributions in the form of
plugins.  The API is not yet stable, and I haven't implemented dynamic plugin
loading yet.  I will, however, update the README when this changes, and provide
documentation for creating plugins.


License
-------

This project is under the Revised BSD license.  Please see
[`LICENSE.txt`](LICENSE.txt) for details.
