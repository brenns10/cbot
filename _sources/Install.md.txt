Install
=======

CBot doesn't have any distribution packaging and doesn't currently do versioned
releases. It also doesn't yet have a standard "install to root" system. So
really this is more of a guide to compiling and running the bot.

Dependencies
------------

See the `meson.build` file for the exhaustive list of libraries and packages we
depend on. But here's a few major ones:

* meson & ninja: build system! (see below)
* gcc or maybe clang
* libconfig: configuration library
* libssl: openssl (used for some crypto/hashing operations)
* libcurl: HTTP and the kitchen sink
* libircclient: duh

### Ubuntu

This will install the minimal set of packages, the rest of the dependencies can
be downloaded and installed from source automatically.

    sudo apt install meson ninja-build gcc libconfig-dev libssl-dev \
        libcurl4-openssl-dev libsqlite3-dev

*Note:* You could use another `libcurl4-dev` provider, but since I already link
directly to OpenSSL, that one is recommended.

### Arch Linux

    sudo pacman -Sy meson ninja gcc libconfig openssl curl sqlite

### macOS

I mean it should probably work with some combination of homebrew and the above
packages. Pull requests welcome.

Building
--------

This project uses the Meson build system. The following commands will build the
project:

    # creates a build directory named "build"
    meson build
    # change to "build" and compile everything
    ninja -C build

Meson will automatically download and build the following dependencies, if they
are not installed on your system:

- `sqlite3`: this is included in the dependency list above but it can be
  downloaded and compiled from source here as well. I'd recommend a system-level
  installation so you can use the `sqlite` command to inspect the database.
- `libircclient`: probably best to let meson download this.
- `sc-regex`, `sc-collections`, `sc-argparse`, `sc-lwt`: these are my personal
  libraries and are not distribution packaged. Just let meson download them!

All build outputs land in the buld directory (use the default name "build").

Running
-------

    build/cbot sample.cfg

The main program takes one argument, a configuration file. The sample file will
drop you into a CLI-based simulation (which you will need to use Ctrl-C to
exit).

To connect to an IRC server, make a copy of `sample.cfg`, read the commented
documentation, and make the necessary edits. If you have any trouble reach out
via GitHub issues or IRC if you know where to find me.
