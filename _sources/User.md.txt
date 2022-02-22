# Users

So you're in a chat with CBot. What can it do? Maybe you've been brought here by
the "cbot help" command. This document explains what you need to know as a user,
including the commands you might be able to use, as well as general guidance.

## Talking to CBot

CBot listens to every message sent in a channel/group, as well as all direct
messages. However, most commands require that you send a message "to" CBot,
either by starting off the message with its name (e.g. "cbot help") or by
tagging the bot using the messaging platform (e.g. "@cbot help"). Direct
messages do not currently count as a "to" CBot message, so you'll need to prefix
those messages with "cbot " as well.

## Plugins

There are a lot of plugins implemented. But not every plugin gets enabled when
CBut runs: the exact list of plugins depends on the configuration used. Ask the
person who runs your CBot instance if you're curious about the details of your
configuration.

This section will be divided into two groups: plugins that are commonly enabled,
and then plugins that are commonly disabled.

### Commonly Enabled Plugins

#### aqi

```
me> cbot aqi
cbot> AQI: 30
```

This plugin looks up the current AQI in San Francisco.

#### birthday

```
me> cbot birthday add 7/4 America
cbot> Ok, I will wish "America" happy birthday on 7/4

me> cbot birthday list
cbot> All birthdays
3/14: Pi
3/17: Saint Patrick
7/4: America
12/25: Jesus

me> cbot birthday remove Pi
cbot> Deleted 1 birthday records
```

This plugin allows you to add, list, and remove birthdays. At a configured time
(likely 9am Pacific), the bot will wish the person a happy birthday in a
configured channel/group.

On the last day of the month, the plugin will also send a listing of next
month's birthdays.

#### greet

```
me> hello cbot
cbot> hello, @me!
```

It just responds to "hello" messages.

#### help

```
me> cbot help
cbot>  << HELP VOMIT >>
```

Prints out a bunch of messages with "help" for using cbot. It's not very useful
and will be replaced with a link to this page.

#### karma

```
me> Stephen is so good at making plugins
me> Stephen++
me> cbot karma Stephen
cbot> Stephen has 1 karma
me> Only one karma? Stephen--
me> cbot karma Stephen
cbot> Stephen has 0 karma
```

Use `++` to add one to a word's karma, and `--` to subtract. Query the karma of
a particular word, or use `cbot karma` to see the highest.

#### reply

This is a generic plugin, so it's difficult to give specific commands.
Basically, you can configure CBot to choose a random response to a message which
matches a pattern. If you've used Slackbot responses before, it's like that.
Unfortunately, this means that there's no standard set of responses: it's all up
to the configuration.

That said, here's a few common ones that I usually configure:

- good morning/evening/afternoon/night -> good X
- lod X -> X: ಠ_ಠ
- ping -> pong
- magic8 -> Magic 8 ball response
  - Alternative syntax is `!8ball`
- cbot sucks (and other variants) -> stupid replies

#### know

```
me> cbot know that Taylor Swift is awesome
cbot> ok, Taylor Swift is awesome
me> cbot what is Taylor Swift
cbot> Taylor Swift is awesome
```

#### weather

```
me> cbot weather 94105
cbot> 94105: 🌦   🌡️+48°F 🌬️→19mph
```

Given a ZIP code or other location, replies with an emoji-rich weather report on
the current conditions. Straight from [wttr.in](http://wttr.in).

### Plugins Not Frequently Used

- annoy: `cbot annoy <user>` sends a message to user every few seconds. It's
  actually mostly a cool demonstration of how to use some technical features of
  the plugin code -- not a good plugin though!
- become: `cbot become <username>` tells cbot to change username. This only
  works for IRC. It's not really a good power to have sitting around though.
  Best to keep disabled.
- emote: `cbot emote <some message>`. An uninteresting command that just has
  CBot repeat a message using the `/me` syntax of IRC
- ircctl: Allows you to instruct CBot to perform IRC-specific actions. Intended
  to allow a user to give CBot higher privileges in the channel, and then
  instruct it to do commands an their behalf.
- log: Supposedly logs channel messages to a file. I haven't used this one in
  years (promise!). Not sure whether it works.
- who: Lists every user in a channel. Not generally good because it sends
  notifications to everyone. It's a useful demonstration of CBot's features to
  list users in a channel though.

There are a few other plugins with abilities even more mundane than the above.
They're just not that interesting.
