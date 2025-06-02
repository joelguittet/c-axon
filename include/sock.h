/**
 * @file      sock.h
 * @brief     Creation and handling of the sockets
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

#ifndef __SOCK_H__
#define __SOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdint.h>
#include <semaphore.h>

/******************************************************************************/
/* Definitions                                                                */
/******************************************************************************/

/* sock_send options */
#define SOCK_SEND_BROADCAST   -1 /* Send data to all connected clients and servers */
#define SOCK_SEND_ROUND_ROBIN -2 /* Send data to the next connected client or server (Round-Robin mechanism) */

/* Sock worker structure */
struct sock_s;
typedef struct sock_worker_s {
    struct sock_s        *parent; /* Parent sock instance */
    struct sock_worker_s *prev;   /* Previous worker instance */
    struct sock_worker_s *next;   /* Next worker instance */
    pthread_t             thread; /* Thread handle of the worker */
    union {
        struct {
            int      socket; /* Listenner socket */
            uint16_t port;   /* Listenner port */
            fd_set   fds;    /* Listenner FDs (myself + all connected clients) */
        } listenner;
        struct {
            int      socket;   /* Reader socket */
            char    *hostname; /* Reader hostname */
            uint16_t port;     /* Reader port */
            fd_set   fds;      /* Reader FDs (myself) */
        } reader;
        struct {
            int    socket; /* Messenger socket */
            void  *buffer; /* Messenger buffer */
            size_t size;   /* Messenger buffer size */
        } messenger;
        struct {
            int    socket; /* Sender socket, can be SOCK_SEND_BROADCAST or SOCK_SEND_ROUND_ROBIN */
            void  *buffer; /* Sender buffer */
            size_t size;   /* Sender buffer size */
        } sender;
    } type;
} sock_worker_t;

/* Worker daisy chain structure */
typedef struct {
    sock_worker_t *first; /* First worker of the daisy chain */
    sock_worker_t *last;  /* Last worker of the daisy chain */
    sem_t          sem;   /* Semaphore used to protect daisy chain */
} sock_worker_list_t;

/* Sock instance structure */
typedef struct sock_s {
    sock_worker_list_t listenners; /* List of listenners */
    sock_worker_list_t readers;    /* List of readers */
    sock_worker_list_t messengers; /* List of messengers */
    sock_worker_list_t senders;    /* List of senders */
    struct {
        int    index; /* Round-Robin index */
        fd_set fds;   /* All clients sockets */
        sem_t  sem;   /* Semaphore used to protect clients */
    } clients;
    struct {
        struct {
            void (*fct)(struct sock_s *, uint16_t, void *); /* Callback function invoked when socket is bound */
            void *user;                                     /* User data passed to the callback */
        } bind;
        struct {
            void (*fct)(struct sock_s *, void *, size_t, int, void *); /* Callback function invoked when message is received */
            void *user;                                                /* User data passed to the callback */
        } message;
        struct {
            void (*fct)(struct sock_s *, char *, void *); /* Callback function invoked when an error occured*/
            void *user;                                   /* User data passed to the callback */
        } error;
    } cb;
} sock_t;

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Function used to create a sock instance
 * @return Sock instance if the function succeeded, NULL otherwise
 */
sock_t *sock_create(void);

/**
 * @brief Bind a new socket to the wanted port
 * @param sock Sock instance
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_bind(sock_t *sock, uint16_t port);

/**
 * @brief Connect a new socket to the wanted host and port
 * @param sock Sock instance
 * @param hostname Hostname
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_connect(sock_t *sock, char *hostname, uint16_t port);

/**
 * @brief Check if sock is already connected to the wanted host and port
 * @param sock Sock instance
 * @param hostname Hostname
 * @param port Port
 * @return true if already connected, false otherwise
 */
bool sock_is_connected(sock_t *sock, char *hostname, uint16_t port);

/**
 * @brief Register callbacks
 * @param sock Sock instance
 * @param topic Topic
 * @param fct Callback function
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_on(sock_t *sock, char *topic, void *fct, void *user);

/**
 * @brief Function used to send data
 * @param sock Sock instance
 * @param buffer Buffer to be sent
 * @param size Size of buffer to send
 * @param socket Socket to which the data should be sent, SOCK_SEND_BROADCAST or SOCK_SEND_ROUND_ROBIN
 * @return 0 if the function succeeded, -1 otherwise
 */
int sock_send(sock_t *sock, void *buffer, size_t size, int socket);

/**
 * @brief Release sock instance
 * @param sock Sock instance
 */
void sock_release(sock_t *sock);

#ifdef __cplusplus
}
#endif

#endif /* __SOCK_H__ */
