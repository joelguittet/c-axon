# c-axon

[![CMake Badge](https://github.com/joelguittet/c-axon/workflows/CMake%20+%20SonarCloud%20Analysis/badge.svg)](https://github.com/joelguittet/c-axon/actions)
[![Issues Badge](https://img.shields.io/github/issues/joelguittet/c-axon)](https://github.com/joelguittet/c-axon/issues)
[![License Badge](https://img.shields.io/github/license/joelguittet/c-axon)](https://github.com/joelguittet/c-axon/blob/master/LICENSE)

[![Bugs](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=bugs)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)
[![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=code_smells)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)
[![Duplicated Lines (%)](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=duplicated_lines_density)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)
[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=ncloc)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)
[![Vulnerabilities](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=vulnerabilities)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)

[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=sqale_rating)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_c-axon&metric=security_rating)](https://sonarcloud.io/dashboard?id=joelguittet_c-axon)

Message-oriented socket library in C.

This repository is not a fork of [axon](https://github.com/tj/axon) ! It has the same behavior but it is a complete library written in C in order to be portable and used in various applications. The goal of this library is to be fully compatible with axon Node.js version and permit pub/sub/push/pull/req/rep communications between applications written in C and Javascript.

## Features

*   message oriented
*   automated reconnection
*   light-weight wire protocol
*   mixed-type arguments (strings, objects, buffers, etc)

## Patterns

*   push / pull
*   pub / sub
*   req / rep

## Building

Build `libaxon.so` with the following commands:

``` bash
cmake .
make
```

## Protocol

Axon uses the extremely simple AMP protocol to send messages on the wire.

The `libaxon.so` library depends of `libamp.so` from [c-amp](https://github.com/joelguittet/c-amp).

## Compatibility

This library is compatible with [axon](https://github.com/tj/axon) release 2.0.3.

## Examples

Several examples are available in the `examples\` directory.

Build examples with the following commands:
``` bash
cmake -DENABLE_AXON_EXAMPLES=ON .
make
```

### Push / Pull

Push instances distribute messages to connected Pull clients/servers using a Round-Robin mechanism. Messages are sent each time to the next available socket. Push instances have also a queing mechanism to handle loss of connections and send them upton the next connection.

Pull instances receives messages from Push servers/clients.

Both Push and Pull instances can be server (binding a socket) or client (connecting to a socket).

### Pub / Sub

Pub instances distribute messages to all connected Sub clients/servers using a broadcast mechanism. Messages are sent each time to all available sockets. Pub instances have no queing mechanism.

Sub instances receives messages from Pub servers/clients.

Sub instances may optionally subscribe to one or more "topics" (the first multipart value), using string patterns or regular expressions.
 
Both Pub and Sub instances can be server (binding a socket) or client (connecting to a socket).

### Req / Rep

Req instances distribute messages to connected Rep clients/servers using a Round-Robin mechanism. Messages are sent each time to the next available socket. Req instances have also a queing mechanism to handle loss of connections and send them upton the next connection. A timeout is specified to wait the reply.

Rep instances receives messages from Req servers/clients and reply (or not).

Both Req and Rep instances can be server (binding a socket) or client (connecting to a socket).

## Performances

Performances have not been evaluated yet.

## What's it good for?

This goal of this library is to provide a C implementation of the original Axon Node.js version. This allow a communication between processes written using different languages.

## API

### axon_t *axon_create(char *type)

Create a new Axon instance of type `type` which is "push", "pull", "pub", "sub", "req" or "rep".

### int axon_bind(axon_t *axon, uint16_t port)

Bind Axon instance on the wanted port. This create a new socket listenning for client connections.

### int axon_connect(axon_t *axon, char *hostname, uint16_t port)

Connect to the wanted host and port. This create a new socket and try to connect to a server. IF the connection can't be established or fail, reconnection is performed.

### int axon_on(axon_t *axon, char *topic, void *fct, void *user)

Register a callback `fct` on the event `topic`. An optionnal `user` argument is available.

| Topic   | Callback                                                | Description                     |
|---------|---------------------------------------------------------|---------------------------------|
| message | amp_msg_t *(*fct)(struct axon_s *, amp_msg_t *, void *) | Called when message is received |
| error   | void *(*fct)(struct discover_s *, char *, void *)       | Called when an error occured    |

### int axon_subscribe(axon_t *axon, char *topic, void *fct, void *user)

Subscribe a callback `fct` on the `topic`. An optionnal `user` argument is available. Can be called to update a subscription.

### int axon_unsubscribe(axon_t *axon, char *topic)

Unsubscribe to the `topic`.

### int axon_send(axon_t *axon, int count, amp_type_e type1, void *value1, ...)

Send data. The `count` value indicate the amount of fields. `type1` and `value1` are the type and value of the first field. `...` expects the other fields with type and value for each of them. Requester should terminate the list of argument by an `amp_msg_t **` to receive the response from the Replier and a timeout `int` value.

### int axon_vsend(axon_t *axon, int count, amp_type_e type1, void *value1, va_list params)

Send data. The `count` value indicate the amount of fields. `type1` and `value1` are the type and value of the first field. `params` expects the other fields with type and value for each of them. Requester should terminate the list of argument by an `amp_msg_t **` to receive the response from the Replier and a timeout `int` value.

### amp_msg_t *axon_reply(axon_t *axon, int count, ...)

Format a new reply (Replier instances only). The `count` value indicate the amount of fields. `...` expects the fields with type and value for each of them.

### void axon_release(axon_t *axon)

Release internal memory and stop Axon instance. All sockets are closed. Must be called to free ressources.

## License

MIT
