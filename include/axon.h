/**
 * @file      axon.h
 * @brief     Axon library
 *
 * MIT License
 *
 * Copyright joelguittet and c-axon contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __AXON_H__
#define __AXON_H__

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__

/* When compiling for windows, we specify a specific calling convention to avoid issues where we are being called from a project with a different default calling convention.  For windows you have 3 define options:

AXON_HIDE_SYMBOLS - Define this in the case where you don't want to ever dllexport symbols
AXON_EXPORT_SYMBOLS - Define this on library build when you want to dllexport symbols (default)
AXON_IMPORT_SYMBOLS - Define this if you want to dllimport symbol

For *nix builds that support visibility attribute, you can define similar behavior by

setting default visibility to hidden by adding
-fvisibility=hidden (for gcc)
or
-xldscope=hidden (for sun cc)
to CFLAGS

then using the AXON_API_VISIBILITY flag to "export" the same symbols the way AXON_EXPORT_SYMBOLS does

*/

#define AXON_CDECL   __cdecl
#define AXON_STDCALL __stdcall

/* export symbols by default, this is necessary for copy pasting the C and header file */
#if !defined(AXON_HIDE_SYMBOLS) && !defined(AXON_IMPORT_SYMBOLS) && !defined(AXON_EXPORT_SYMBOLS)
#define AXON_EXPORT_SYMBOLS
#endif

#if defined(AXON_HIDE_SYMBOLS)
#define AXON_PUBLIC(type) type AXON_STDCALL
#elif defined(AXON_EXPORT_SYMBOLS)
#define AXON_PUBLIC(type) __declspec(dllexport) type AXON_STDCALL
#elif defined(AXON_IMPORT_SYMBOLS)
#define AXON_PUBLIC(type) __declspec(dllimport) type AXON_STDCALL
#endif
#else /* !__WINDOWS__ */
#define AXON_CDECL
#define AXON_STDCALL

#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined(__SUNPRO_C)) && defined(AXON_API_VISIBILITY)
#define AXON_PUBLIC(type) __attribute__((visibility("default"))) type
#else
#define AXON_PUBLIC(type) type
#endif
#endif

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>

#include "amp.h"

/******************************************************************************/
/* Definitions                                                                */
/******************************************************************************/

/* Axon type */
typedef enum {
    AXON_TYPE_PUB, /* Publisher (server which is broadcasting data to all its clients OR a client which is broadcasting data to all its servers) */
    AXON_TYPE_SUB, /* Subscriber (client receiving broadcasted data from the Publisher server OR server receiving broadcasted data from the Publisher client) */
    AXON_TYPE_PUSH, /* Pusher (server which is sending data to its clients OR a client which is sending data to its servers - both with Round-Robin mechanism and queing) */
    AXON_TYPE_PULL, /* Puller (client receiving data data from the Pusher server OR server receiving data from the Pusher client) */
    AXON_TYPE_REQ, /* Requester (client sending requests and receiving responses from the Replier server OR server sending requests and receiving responses from the Replier client - both with Round-Robin mechanism and queing) */
    AXON_TYPE_REP /* Replier (server waiting for message from clients and replying to the client OR client waiting for message from servers and replying to the server) */
} axon_enum_e;

/* Axon topic subscription */
struct axon_s;
typedef struct axon_sub_s {
    struct axon_sub_s *next;                                         /* Next subscription */
    char              *topic;                                        /* Topic of the subscription */
    amp_msg_t *(*fct)(struct axon_s *, char *, amp_msg_t *, void *); /* Callback function invoked when topic is received */
    void *user;                                                      /* User data passed to the callback */
} axon_sub_t;

/* Axon instance */
typedef struct sock_s sock_t;
typedef struct axon_s {
    axon_enum_e  type;   /* Axon instance type */
    sock_t      *sock;   /* Sock instance */
    unsigned int msg_id; /* Requester message ID used to retrieve response */
    struct {
        axon_sub_t *first; /* Topic subscription daisy chain */
        sem_t       sem;   /* Semaphore used to protect daisy chain */
    } subs;
    struct {
        struct {
            void *(*fct)(struct axon_s *, uint16_t, void *); /* Callback function invoked when socket is bound */
            void *user;                                      /* User data passed to the callback */
        } bind;
        struct {
            amp_msg_t *(*fct)(struct axon_s *, amp_msg_t *, void *); /* Callback function invoked when message is received */
            void *user;                                              /* User data passed to the callback */
        } message;
        struct {
            void *(*fct)(struct axon_s *, char *, void *); /* Callback function invoked when an error occurs */
            void *user;                                    /* User data passed to the callback */
        } error;
    } cb;
} axon_t;

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Function used to create axon instance
 * @param type Type of Axon instance
 * @return Axon instance if the function succeeded, NULL otherwise
 */
AXON_PUBLIC(axon_t *) axon_create(char *type);

/**
 * @brief Bind axon on the wanted port
 * @param axon Axon instance
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
AXON_PUBLIC(int) axon_bind(axon_t *axon, uint16_t port);

/**
 * @brief Connect axon to the wanted host and port
 * @param axon Axon instance
 * @param hostname Hostname
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
AXON_PUBLIC(int) axon_connect(axon_t *axon, char *hostname, uint16_t port);

/**
 * @brief Check if axon is already connected to the wanted host and port
 * @param axon Axon instance
 * @param hostname Hostname
 * @param port Port
 * @return true if already connected, false otherwise
 */
AXON_PUBLIC(bool) axon_is_connected(axon_t *axon, char *hostname, uint16_t port);

/**
 * @brief Register callbacks
 * @param axon Axon instance
 * @param topic Topic
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
AXON_PUBLIC(int) axon_on(axon_t *axon, char *topic, void *fct, void *user);

/**
 * @brief Subscribe to wanted topic
 * @param axon Axon instance
 * @param topic Topic
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
AXON_PUBLIC(int) axon_subscribe(axon_t *axon, char *topic, void *fct, void *user);

/**
 * @brief Unubscribe to wanted topic
 * @param axon Axon instance
 * @param topic Topic
 * @return 0 if the function succeeded, -1 otherwise
 */
AXON_PUBLIC(int) axon_unsubscribe(axon_t *axon, char *topic);

/**
 * @brief Function used to send data to the server or to all connected clients
 * @param axon Axon instance
 * @param count Amount of data to be sent
 * @param type1 Type of the first field of the message
 * @param value1 Value  of the first field of the message
 * @param ... type, data Array of type and data (and size for blob type) to be sent, AMP response message and timeout for Requester instance
 * @return 0 if the function succeeded, -1 otherwise
 */
AXON_PUBLIC(int) axon_send(axon_t *axon, int count, amp_type_e type1, void *value1, ...);

/**
 * @brief Function used to send data to the server or to all connected clients
 * @param axon Axon instance
 * @param count Amount of data to be sent
 * @param type1 Type of the first field of the message
 * @param value1 Value  of the first field of the message
 * @param params type, data Array of type and data (and size for blob type) to be sent, AMP response message and timeout for Requester instance
 * @return 0 if the function succeeded, -1 otherwise
 */
AXON_PUBLIC(int) axon_vsend(axon_t *axon, int count, amp_type_e type1, void *value1, va_list params);

/**
 * @brief Function used by Replier instance to format response to the server or to a single client
 * @param axon Axon instance
 * @param count Amount of data to be sent
 * @param ... type, data Array of type and data (and size for blob type) to be sent
 * @return AMP response message
 */
AXON_PUBLIC(amp_msg_t *) axon_reply(axon_t *axon, int count, ...);

/**
 * @brief Release axon instance
 * @param axon Axon instance
 */
AXON_PUBLIC(void) axon_release(axon_t *axon);

#ifdef __cplusplus
}
#endif

#endif /* __AXON_H__ */
