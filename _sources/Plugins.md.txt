How to write a CBot Plugin
==========================

CBot has a quite pleasant plugin system. If you're familiar with C, you're able
to extend the bot quite a bit. Follow this guide to learn the basics of CBot
plugin development.

To drive this tutorial, we will try to build a "hello world" plugin. This plugin
will simply reply to any message which says "hello" and respond "world". While
the functionality itself is pretty silly, once you can do this, you'll be able
to start making much more interesting stuff.

As a prerequisite, this guide assumes you are able to use the build system to
compile CBot.

Step 1: Plugin structure
------------------------

The best way to write a CBot plugin is "in-tree", that is by writing it within
this repository. Just create a file "plugin/hello.c".

**Aside:** whatever name you choose for your plugin (not including the `.c` file
extension) should be a valid C identifier, more or less. This will allow it to
be included in the configuration file.

Here is a completely blank, minimal plugin, which does nothing:

    #include "cbot/cbot.h"

    static int load(struct cbot_plugin *plugin, config_setting_t *conf)
    {
        return 0;
    }

    struct cbot_plugin_ops ops = {
        .load=load,
    };

Save that as your plugin file, and then add a your plugin name to the `plugins`
list in the `meson.build` file at the root of this repository. When you
recompile (`ninja -C build`), the build system will detect a change and
reconfigure itself. Then it should compile everything, along with your new
plugin. You'll find the compiled plugin at `build/hello.so`.

Step 2: ops Struct and load()
-----------------------------

The above plugin file contained an empty function and a struct declaration.
These are the minimal items for a plugin. When CBot loads your plugin, it
searches for a symbol named `ops`, and expects it to be of type `struct
cbot_plugin_ops`. You can see the definition of this type in `inc/cbot/cbot.h`,
but here it is with docstrings removed:

    struct cbot_plugin_ops {
        char *description;
        int (*load)(struct cbot_plugin *plugin, config_setting_t *config);
        void (*unload)(struct cbot_plugin *plugin);
        void (*help)(struct cbot_plugin *plugin, struct sc_charbuf *cb);
    };

All of the fields are optional, except for `load`, which is the one we'll focus
on right now. This function should do any initialization tasks for the plugin.
This is the place where a plugin should tell the bot that it would like to
respond to certain types of messages or events. *If you don't register any
handlers for events here, the plugin will never get called again.* So, our empty
load() function above is a bit silly, but it's perfectly valid. The plugin
simply does nothing.

There are two important arguments to the load() function: a `struct cbot_plugin`
pointer, and a `config_setting_t` pointer. The plugin struct looks like this:

    struct cbot_plugin {
        struct cbot_plugin_ops *ops;
        void *data;
        struct cbot *bot;
    };

An instance of this struct is allocated for each instance of the plugin which is
loaded. The `ops` pointer should simply point to the variable defined at the
bottom of the file. `data` will be NULL, but can be used by the plugin to store
data for later. Finally, the `bot` pointer points to the main struct of `cbot`,
which is used for most bot actions like sending messages.

The plugin may modify any field of the plugin struct as it wants. CBot will use
the `ops` that are currently in the plugin struct, so if the bot would like to
dynamically change behavior, that's ok. CBot does not rely on the `bot` pointer
within the plugin, but it's in a plugin's best interest to leave that alone. If
a plugin overwrites it, calling API functions will become much more difficult.

The second argument is a `config_setting_t *`, which is an element from
[libconfig](http://hyperrealm.github.io/libconfig/). I won't cover that at all,
except to mention that this points to plugin-specific configuration, and can be
accessed using any libconfig function you want.

Step 3: Registering handlers
----------------------------

Now that we know what load() is, let's use it to register a "handler". CBot lets
us handle a few different types of events (see `enum cbot_event_type`) but the
most important two are `CBOT_MESSAGE` and `CBOT_ADDRESSED`. `CBOT_MESSAGE` is an
event which triggers on every single message: direct message, channel message,
etc. `CBOT_ADDRESSED` is a special event which only triggers on a subset of
messages: those which are "addressed" to the bot. Here are some examples of
"addressed" messages:

* Message in a channel: `cbot: hello!`
* Message in a channel: `cbot hello!`
* Direct message to cbot: `hello!`

All three of the above messages would generate the same `CBOT_ADDRESSED` event,
and the contents of the message would be trimmed to remove the bot's name. This
allows plugin authors to respond to messages directed at the bot, without
knowing about the bot's current username, and without having to filter out
irrelevant messages.

In our case, we want to respond to any message which says "hello" -- not just
those which are addressed to us. So, we should register a handler for
`CBOT_MESSAGE`.

For message events, CBot lets you specify a regular expression to filter
messages further by their contents. Your handler will only be called if the full
contents of the message matches this regex. We will simply specify a regex of
`hello` to make our plugin nice and easy.

A handler function, to CBot, is a function which takes a two arguments:

- `struct cbot_event *event`: this is a general purpose event struct. Different
  types of events have more specialized versions with different fields. The
  event struct has a `bot` field, a `plugin` field, and a `type` field which
  tells you the event type. For message events, you can cast this to a `struct
  cbot_message_event` which further contains strings `channel`, `username`,
  `message`, and some other fields we won't discuss.
- `void *user`: this is given to CBOT when you register the handler. It can be
  used to store handler-specific data.

With this information, the handler registration function should be clear:

    struct cbot_handler *cbot_register(struct cbot_plugin *plugin,
                                       enum cbot_event_type type,
                                       cbot_handler_t handler, void *user,
                                       char *regex);

The first argument is the plugin doing the registration. type is the event type,
handler is the function to be called, user is the user data, and regex is the
(optional) regex to filter messages by. Let's put this together and register a
hypothetical handler:

    static int load(struct cbot_plugin *plugin, config_setting_t *conf)
    {
        cbot_register(plugin, CBOT_MESSAGE, (cbot_handler_t)say_hello, NULL,
                      "hello");
        return 0;
    }

Step 4: Handler function
------------------------

Finally, let's look at the handler function for our plugin:

    static void say_hello(struct cbot_message_event *event, void *user)
    {
        cbot_send(event->bot, event->channel, "world");
    }

First, note that our first argument is not `struct cbot_event`, it is `struct
cbot_message_event`. We can do this because our handler only handles messages,
but the downside is that we had to cast this function to `(cbot_handler_t)`
when it was registered above.

The remainder of this function seems pretty obvious: send the message "world" to
the channel in which the original message came in from.

Step 5: Compile and run
-----------------------

Now that we have a complete plugin, we should compile it and run it. Use `ninja
-C build` to compile the plugin. Running it is as simple as adding a line in the
`plugins` group of your config file:

    hello: {};

Notice the empty `{}` group -- any configuration inside of this would go
directly to the `conf` variable in the `load()` function -- neat, right?

Anyway, if you don't have a configuration yet, this would be a good baseline to
start testing outside of IRC. Save it as `cli.cfg`. You can add more plugins
too, if you so desire.

    cbot: {
      name = "cbot";
      channels = (
        { name = "stdin" },
      );
      backend = "cli";
      plugin_dir = "build";
      db = "cli.sqlite3";
    };
    cli: {};
    plugins: {
      hello: {};
    };

After compiling, run `build/cbot cli.cfg` and you should have a CLI chat with
cbot in it. If you see the following message (with no errors after it), then you
know your plugin has been loaded:

    attempting to load symbol ops from build/hello.so

You should be able to type "hello" and get a response from cbot:

    > hello
    [stdin]cbot: world
    >

And that's it, you've written your first plugin!

Next up, take a browse through [Plugins-2.md](Plugins-2.md) to learn some
advanced topics and APIs which help you make great plugins.
