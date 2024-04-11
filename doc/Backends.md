# CBot Backends

The backend is the part of CBot that allows the bot to communicate with whatever
chat system it is using. Currently, that is IRC, Signal (via signald), or the
console for testing. This page gives basic information about the backend
responsibilities / APIs, and also information on selected backend
implementations.

## APIs

Backends must populate a `struct cbot_backend_ops` with function pointers to
valid implementations. See the `cbot_private.h` header for details on these
function pointers.

## Signald backend

Communicates with [signald](https://signald.org/) over a JSON API on a Unix
socket. Signald has some pretty decent documentation on the structures and
formats of each request.
