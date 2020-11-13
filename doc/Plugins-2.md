Plugins - Advanced Topics
=========================

This document is not particularly detailed. It's intended to give you an idea
for the tools that CBot contains to help you write plugins. Rather than give
detailed code examples, I'll point to plugins you should read to learn more.

APIs To Learn
-------------

Check out the following libraries (source located at
https://sr.ht/~brenns10/sc-libs) for APIs commonly used in CBot:

* `sc-collections`: we heavily use the linked list and character buffer, docs
  are in the `sc-collections.h` header.
* `sc-regex`: you mostly care about the API for getting captured strings,
  `sc_regex_get_capture()`
* `sc-lwt`: the API is not that large, read the header file

Persistent Storage: Sqlite Database
-----------------------------------

CBot links to Sqlite and maintains a persistent database file. Plugins are
welcome to use it to provide persistence. CBot provides a couple convenience
tools, but the authoritative source is the [sqlite docs][1].

[1]: https://sqlite.org/docs.html

The following plugins make use of the database and would be a good reference for
the tools documented below:

- [plugin/sqlkarma.c](../plugin/sqlkarma.c)
- [plugin/sqlknow.c](../plugin/sqlknow.c)

### Table registration and migration

A plugin should "register" a table with a `struct cbot_db_table`. Why? The
struct contains a version field, and CBot maintains a schema version table. If
you decide to change the schema for a table, you just increment the version, and
the bot will know the table in its database is out of date. Then, it will look
into an array of `ALTER TABLE` statements you provide, which will be able to
migrate the old schema to the new one.

### Query functions

Running queries in sqlite is a bit tedious, so CBot contains a macro system to
allow you to generate functions which execute a query and return a result. I
won't bother to go into many details, just read
[inc/cbot/db.h](../inc/cbot/db.h).

Lightweight Threads
-------------------

It may not be obvious, but CBot is actually an "async" program, which makes use
of a pretty awesome lightweight threading model. There's only one "operating
system thread", but CBot itself can switch between several "lightweight threads"
(which I refer to as lwts in code/docs) whenever they have no work to be done.
This is cooperative multitasking.

The IRC operations and plugins execute all one one lightweight thread.
Typically, that thread is blocked, waiting for I/O from IRC, so there's plenty
of opportunity for other lwts to run.

Plugins can use `cbot_get_lwt_ctx()` to retrieve a context object for use with
the sc-lwt library, and then they can go ahead and launch lightweight threads.
Note that plugins should behave well and not hog the CPU. This is mainly meant
for other I/O operations.

Note that if a plugin would like to call ANY function which would block (such as
libcurl) then it should make sure that it is async-safe and integrated with
sc-lwt, and they need to do it off the main lwt.

A great basic lwt example is [plugin/annoy.c](../plugin/annoy.c).

HTTP Requests: libcurl
----------------------

[Docs](https://curl.se/libcurl/c/)

libcurl is an excellent HTTP library! It is integrated with CBot now so you can
use it to make plugins which connect to APIs etc. CBot uses the async mode of
libcurl (the Multi API) along with sc-lwt to be able to write code which can run
several HTTP requests in parallel, all on the same OS-thread (different LWTs)
without writing tons of callbacks.

* In order to use libcurl, you MUST create a separate lwt and do all blocking
  actions on that thread.
* Create an "easy handle" (see [cURL easy overview][2]) for your request, but
  rather than using `curl_easy_perform()` use `cbot_curl_perform()` (see
  [inc/cbot/curl.h](../inc/cbot/curl.h)).

[2]: https://curl.se/libcurl/c/libcurl-easy.html

Check out [plugin/weather.c](../plugin/weather.c) for a curl example.

Tokenizing
----------

Sometimes you just want to write a command that takes, you know, an array of
arguments. Tokenizing in C can be a drag. CBot has a tokenizer that takes a
string, and gives you an array of tokens which were delimited by whitespace. The
tokenizer supports basic quoting to allow whitespace in your arguments, and
that's about it.

Check out [plugin/tok.c](../plugin/tok.c) to see the tokenizer in action.
It's implemented in [src/tok.c](../src/tok.c), which is worth reading.

Dynamic Formatting
------------------

Love Python format() and f-strings? Well, C won't get you anything nearly as
convenient, but this is a good 70%. It's based on the charbuf struct in
sc-collections. It allows you to give a string like this:

    "Hello {target}, my name is {myname}! Check out {url}"

And fill in the curly braces with the correct values. This isn't generally all
that useful in C, because you usually can stick with the printf family. But
sometimes you don't know the format string or the order of the items, or even
what you want to include in your string output.

The `cbot_format()` API takes a formatter function, which gets called for each
curly brace expansion and can append whatever it wants into the string builder /
charbuf.

See it in action in [plugin/reply.c](../plugin/reply.c).
