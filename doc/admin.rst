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
<https://hub.docker.com/r/brenns10/cbot>`_. These Docker images are optimized
for running CBot with Signal, and by default they launch a script which launches
the Signald daemon to allow it to interface with Signal. If you are deploying
CBot on Signal, you may find this a useful deployment method. However, for
deploying to IRC, it may be more convenient to build and run CBot from source.

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
- libircclient: This is for IRC support. It is required, but we can use a
  "vendored" version if your OS doesn't package it.
- sqlite3: Used for maintaining state. It is required, but we can use a vendored
  version as necessary.
- Several libraries from `sc-libs <https://sr.ht/~brenns10/sc-libs/>`_, these
  can all be setup by the build system, as they have no OS packaging.

Here are some commands to get setup for various distributions:

.. code:: bash

   # Ubuntu
   sudo apt install meson gcc libconfig-dev libcurl4-openssl-dev libsqlite3-dev

   # Arch
   sudo pacman -Sy meson gcc libconfig curl sqlite

Building
^^^^^^^^

Once you have setup the dependencies, you'll need to build the project:

This project uses the Meson build system. The following commands will build the
project:

.. code:: bash

    # creates a build directory named "build"
    meson build
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