Administering CBot
==================

Installing (Distribution Packages)
----------------------------------

CBot has source releases on `Github <https://github.com/brenns10/cbot>`_ as well
as binary package releases for Alpine Linux. You can install the Alpine packages
with ``apk add --allow-untrusted <file>``. For other Linux distributions, you'll
need to install from source.

Installing (Docker)
-------------------

CBot also has Docker images published on `Docker Hub
<https://hub.docker.com/r/brenns10/cbot>`_. Since version v0.8.0, these Docker
images no longer contain Signald bundled. If you want to run CBot with Signal,
you'll need to setup Signald yourself, likely via their Docker image.
Docker-Compose is great for this.

Installing (From Source)
------------------------

Dependencies
^^^^^^^^^^^^

First, you'll need to unpack your source release (or checkout a git revision of
interest). Next, setup your dependencies. The following are dependencies of
CBot.

- meson: this is the build system for CBot, it is required (usually a package
  named ``meson``)
- A C compiler (gcc or clang)
- libconfig: This is used to parse the configuration file. It is required.
- libcurl: For HTTP queries to APIs. It is required.
- libmicrohttpd: For HTTP server. It is required
- libircclient: This is for IRC support. It is required, but we can use a
  "vendored" version if your OS doesn't package it.
- sqlite3: Used for maintaining state. It is required, but we can use a vendored
  version as necessary.
- Several libraries from `sc-libs <https://sr.ht/~brenns10/sc-libs/>`_, these
  can all be setup by the build system, as they have no OS packaging.

Here are some commands to get setup for various distributions:

.. code:: bash

   # Ubuntu
   sudo apt install meson gcc libconfig-dev libcurl4-openssl-dev libsqlite3-dev libmicrohttpd-dev

   # Arch
   sudo pacman -Sy meson gcc libconfig curl sqlite libmicrohttpd

Building
^^^^^^^^

Once you have setup the dependencies, you'll need to build the project:

This project uses the Meson build system. The following commands will build the
project:

.. code:: bash

    # creates a build directory named "build"
    meson setup build
    # change to "build" and compile everything
    ninja -C build

During the ``meson build`` step, the build system will download any dependencies
that it cannot find installed on your system, including all of the sc-libs
dependencies.

During ``ninja -C build``, the actual program compilation happens. It should be
snappy.

Initial Run
^^^^^^^^^^^

To test your built program, you can use ``sample.cfg``, which sets up some
plugins in the CLI. Simply run:

.. code:: bash

   build/cbot sample.cfg

Configuring
-----------

A sample configuration file is bundled as ``sample.cfg``. This should be used as
the basis for your configuration. However, there are some very important things
which ought to be documented.

Choosing & Configuring Backend
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the ``cbot`` configuration object, you should find an option ``backend``.
This is required, and you can choose between:

- ``"irc"``
- ``"signal"``
- ``"cli"``

Whichever backend you choose, you must also have a corresponding top-level
configuration object with the same name. This object provides necessary config
for the backend.

For the irc backend, you need to configure:

- host: string hostname (use "#" prefix for SSL)
- port: integer port
- password: string password

For the signal backend, you must run an instance of `Signald
<https://signald.org/>`_. The following configuration is avaialable:

- phone: phone number of bot account
- signald_socket: filename to connect to Signald
- auth: authorized phone number of privileged account
- ignore_dm: should the bot ignore DMs?

Configuring CBot
^^^^^^^^^^^^^^^^

Beyond the backend configuration, the ``plugins`` top-level configuration option
specifies each plugin which is loaded. Each plugin named in this mapping will be
loaded. The plugin can be mapped to an empty config dict for plugins with no
options, for example:

.. code::

    help: {};

If the plugin requires configuration, it can be provided in these objects.
Please see ``sample.cfg`` for examples of each plugin's configuration.

Plugins
-------

This section documents the administration side of some plugins, in case they
need a bit more explanation.

Trivia
^^^^^^

The trivia plugin is a powerful weekly reminder system for the group chat.
Basically, every week at a given time, a message gets sent announcing the trivia
night and requesting RSVP. Then, at the RSVP deadline, a message gets sent to
the trivia host to RSVP a table.

In order to RSVP, users react with any emoji, except for a few sad face emoji
choices. If they react with a numeric emoji, that indicates bringing a guest.

While it's definitely nice to RSVP and reserve a table, the main benefit is
actually for reminding everybody that it's trivia day, and letting everybody see
who else will be joining, without needing to fill the chat with lots of
notifications.

The RSVP message can be sent to trivia host in one of two ways:

1. Email
2. SMS

Strangely, signal is not an option here (but it could be easily added, if it
were needed). The email option was the original, but the SMS option can be
configured with "ntfy.sh" and the android app Tasker to automate the SMS
sending.

Configurations:

- channel: set the channel where trivia is organized (required)
- sendmail_command: set the command to run to send email (or SMS). This command
  is executed via ``popen()``, which means that it is passed to the system
  shell, and the message text is written into the command via stdin.

  - A configuration like ``msmtp -t recipient@example.com`` works well for Email
    based sending.
  - A configuration like ``perl -pe 's/\n/\r\n' - | curl --data-binary @- https://ntfy.sh/TOPIC``
    works for SMS. You'll then need to configure ntfy.sh and the Android Tasker
    app as seen `here <https://docs.ntfy.sh/subscribe/phone/#automation-apps>`_.

- email_format: set this to false when you're using SMS, true for email (default
  true)
- init_hour, init_minute: the start time for trivia
- send_hour, send_miniute: the rsvp time for trivia
- trivia_weekday: day of week (0 - Sunday, 1 - Monday, etc...)
- from_name: string name of sender
- to_name: string name of recipient
- from: sender email address (required when email_format is set)
- to: recipient email address (required when email_format is set)
