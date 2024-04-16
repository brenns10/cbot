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

Here are sample JSON requests:

### Send message

```json
{
    "version": "v1",
    "type": "send",
    "id": "some string",
    "username": "+12345678901",
    "recipientGroupId": "group recipient ID",
    "recipientAddress": {"uuid": "string"},
    "messageBody": "message text here",
    "mentions": [
        {"length": 0, "start": 1, "uuid": "user ID"}
    ]
}
```

* username - the sender (i.e. cbot) phone number. Maybe could become the signal
  username instead?
* either recipientGroupId or recipientAddress is specified

### List Groups

Request

```json
{
    "version": "v1",
    "type": "list_groups",
    "account": "+12345678901"
}
```

### Get user profile

```json
{
    "version": "v1",
    "type": "get_profile",
    "account": "+12345678901",
    "address": {"uuid": "foo", "number": "foo"}
}
```

* address can specify either uuid or phone number

## Signal-CLI backend

Communicates with [signal-cli](https://github.com/AsamK/signal-cli) over the
jsonRpc API. Compared to signald, signal-cli has much less documentation, but it
does seem that the maintenance has kept up with the signal protocol changes
recently. As a result, signal-cli will be a newly added backend for Signal, in
order to have more options for keeping bots running.

Minimal API documentation
[here](https://github.com/AsamK/signal-cli/blob/master/man/signal-cli-jsonrpc.5.adoc).

All requests are JSON objects on one line, with the following keys:

* jsonRpc: version as a string, currently "2.0"
* method: name of the signal-cli command to execute, string
* id: a string that identifies this request for the response
* params: an object containing the request parameters, which are generally the
  same as the CLI options, taking each word and turning it into camelCase.

To aid in development, here are sample JSON requests and responses.

### Incoming Message

From docs:

```json
{
  "jsonrpc": "2.0",
  "method": "receive",
  "params": {
    "envelope": {
      "source": "**REMOVED**",
      "sourceNumber": null,
      "sourceUuid": "**REMOVED**",
      "sourceName": "Stephen Brennan",
      "sourceDevice": 3,
      "timestamp": 1712876927992,
      "dataMessage": {
        "timestamp": 1712876927992,
        "message": "￼",
        "expiresInSeconds": 0,
        "viewOnce": false,
        "mentions": [
          {
            "name": "**REMOVED**",
            "number": "**REMOVED**",
            "uuid": "**REMOVED**",
            "start": 0,
            "length": 1
          }
        ],
        "groupInfo": {
          "groupId": "** GROUP ID *",
          "type": "DELIVER"
        }
      }
    },
    "account": "** REMOVED*"
  }
}

```

### Typing Started

```
{
  "jsonrpc": "2.0",
  "method": "receive",
  "params": {
    "envelope": {
      "source": "**REMVOED (UUID)**",
      "sourceNumber": null,
      "sourceUuid": "**REMOVED (UUID)**",
      "sourceName": "Stephen Brennan",
      "sourceDevice": 3,
      "timestamp": 1712857247046,
      "typingMessage": {
        "action": "STARTED",
        "timestamp": 1712857247046,
        "groupId": "**REMOVED**"
      }
    },
    "account": "**REMOVED (bot phone number)**"
  }
}
```

### Reaction Add

```json
{
  "jsonrpc": "2.0",
  "method": "receive",
  "params": {
    "envelope": {
      "source": "**REMOVED (UUID of sender)**",
      "sourceNumber": null,
      "sourceUuid": "**REMOVED (UUID of sender)**",
      "sourceName": "Stephen Brennan",
      "sourceDevice": 3,
      "timestamp": 1712857469462,
      "dataMessage": {
        "timestamp": 1712857469462,
        "message": null,
        "expiresInSeconds": 0,
        "viewOnce": false,
        "reaction": {
          "emoji": "❤️ ",
          "targetAuthor": "**REMOVED (UUID of author of reacted message)**",
          "targetAuthorNumber": null,
          "targetAuthorUuid": "**REMOVED**",
          "targetSentTimestamp": 1712857247773,
          "isRemove": false
        },
        "groupInfo": {
          "groupId": "**REMOVED**",
          "type": "DELIVER"
        }
      }
    },
    "account": "**REMOVED**"
  }
}
```

* For removals, set "isRemove" to true!

### Group Updates

It looks like any update to the group will trigger a message like this.

- User joins/leaves
- Update group name, picture, link

```json
{
  "jsonrpc": "2.0",
  "method": "receive",
  "params": {
    "envelope": {
      "source": "**REMOVED**",
      "sourceNumber": null,
      "sourceUuid": "**REMOVED**",
      "sourceName": "Stephen Brennan",
      "sourceDevice": 3,
      "timestamp": 1712870390764,
      "dataMessage": {
        "timestamp": 1712870390764,
        "message": null,
        "expiresInSeconds": 0,
        "viewOnce": false,
        "groupInfo": {
          "groupId": "**REMOVED**",
          "type": "UPDATE"
        }
      }
    },
    "account": "**Removed**"
  }
}
```

It would appear this message doesn't actually contain the group info, and you'll
need to query it.

### List Groups

Request:

```json
{
    "jsonrpc": "2.0",
    "method": "listGroups",
    "id": "my special mark"
}
```

Response:

```json
{
  "jsonrpc": "2.0",
  "result": [
    {
      "id": "* id **",
      "name": "* name **",
      "description": "* text desciption *",
      "isMember": true,
      "isBlocked": false,
      "messageExpirationTime": 0,
      "members": [
        {
          "number": null,
          "uuid": "* uuid *"
        }
      ],
      "pendingMembers": [],
      "requestingMembers": [],
      "admins": [
        {
          "number": null,
          "uuid": "* uuid *"
        }
      ],
      "banned": [],
      "permissionAddMember": "EVERY_MEMBER",
      "permissionEditDetails": "EVERY_MEMBER",
      "permissionSendMessage": "EVERY_MEMBER",
      "groupInviteLink": "https://signal.group/#foo"
    },
    {
      "id": "* id **",
      "name": null,
      "description": null,
      "isMember": false,
      "isBlocked": false,
      "messageExpirationTime": 0,
      "members": [],
      "pendingMembers": [],
      "requestingMembers": [],
      "admins": [],
      "banned": [],
      "permissionAddMember": "EVERY_MEMBER",
      "permissionEditDetails": "EVERY_MEMBER",
      "permissionSendMessage": "EVERY_MEMBER",
      "groupInviteLink": null
    }
  ],
  "id": "my special mark"
}
```

### List Contacts

Request:

```json
{"jsonrpc": "2.0", "method": "listContacts", "id": "1"}
```

Response:

```json
{
  "jsonrpc": "2.0",
  "result": [
    {
      "number": null,
      "uuid": "* UUID **",
      "username": null,
      "name": "",
      "color": null,
      "isBlocked": false,
      "messageExpirationTime": 0,
      "profile": {
        "lastUpdateTimestamp": 1712856260058,
        "givenName": "NAME HERE",
        "familyName": null,
        "about": "",
        "aboutEmoji": "",
        "mobileCoinAddress": null
      }
    },
    {
      "number": null,
      "uuid": "* UUID **",
      "username": null,
      "name": "",
      "color": null,
      "isBlocked": false,
      "messageExpirationTime": 0,
      "profile": {
        "lastUpdateTimestamp": 1712856468855,
        "givenName": "Stephen",
        "familyName": "Brennan",
        "about": "",
        "aboutEmoji": "",
        "mobileCoinAddress": null
      }
    }
  ],
  "id": "1"
}
```

### Send Message

```json
{"jsonrpc": "2.0", "method": "send", "id": "1", "params": {"message": "hello 💩💩 hello X!", "groupId": "GROUPID", "mentions": ["15:1:UID"]}}

{"jsonrpc": "2.0", "method": "send", "id": "1", "params": {"message": "hello world", "recipient": "UID"}}
```

### Success

```json
{
  "jsonrpc": "2.0",
  "result": {
    "results": [
      {
        "recipientAddress": {
          "uuid": "UUID",
          "number": null
        },
        "type": "SUCCESS"
      }
    ],
    "timestamp": 1713212326970
  },
  "id": "1"
}
```

### Set Nick

```json
{"jsonrpc": "2.0", "method": "updateProfile", "id": "1", "params": {"givenName": "cbottest"}}
```
