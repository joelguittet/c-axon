/**
 * @file      sock.c
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

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <pthread.h>

#include "sock.h"

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Sock thread used to handle clients connections and reception of data
 * @param arg Sock listenner
 * @return Always returns NULL
 */
static void *sock_thread_listenner(void *arg);

/**
 * @brief Sock thread used to handle server connections and reception of data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *sock_thread_reader(void *arg);

/**
 * @brief Sock thread used to handle data received
 * @param arg Worker
 * @return Always returns NULL
 */
static void *sock_thread_messenger(void *arg);

/**
 * @brief Sock thread used to send data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *sock_thread_sender(void *arg);

/**
 * @brief Start a new worker
 * @param sock Sock instance
 * @param list List of workers to which the new one should be added
 * @param worker Worker to start
 * @param start_routine Worker thread function
 * @return 0 if the function succeeded, -1 otherwise
 */
static int sock_start_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker, void *(*start_routine)(void *));

/**
 * @brief Remove a worker
 * @param sock Sock instance
 * @param list List of workers to which the worker should be removed
 * @param worker Worker to remove
 * @return 0 if the function succeeded, -1 otherwise
 */
static int sock_remove_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker);

/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/

/**
 * @brief Function used to create a sock instance
 * @return Sock instance if the function succeeded, NULL otherwise
 */
sock_t *
sock_create(void) {

    /* Create new sock instance */
    sock_t *sock = (sock_t *)malloc(sizeof(sock_t));
    if (NULL == sock) {
        /* Unable to allocate memory */
        return NULL;
    }
    memset(sock, 0, sizeof(sock_t));

    /* Initialize semaphore used to access listenners */
    sem_init(&sock->listenners.sem, 0, 1);

    /* Initialize semaphore used to access readers */
    sem_init(&sock->readers.sem, 0, 1);

    /* Initialize semaphore used to access messengers */
    sem_init(&sock->messengers.sem, 0, 1);

    /* Initialize semaphore used to access senders */
    sem_init(&sock->senders.sem, 0, 1);

    /* Initialize clients FDs and semaphore */
    sem_init(&sock->clients.sem, 0, 1);
    FD_ZERO(&sock->clients.fds);

    return sock;
}

/**
 * @brief Bind a new socket to the wanted port
 * @param sock Sock instance
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_bind(sock_t *sock, uint16_t port) {

    assert(NULL != sock);

    /* Create new listenner */
    sock_worker_t *worker = (sock_worker_t *)malloc(sizeof(sock_worker_t));
    if (NULL == worker) {
        /* Unable to allocate memory */
        return -1;
    }
    memset(worker, 0, sizeof(sock_worker_t));

    /* Store port, initialize FDs */
    worker->type.listenner.port = port;
    FD_ZERO(&worker->type.listenner.fds);

    /* Start listenner */
    if (0 != sock_start_worker(sock, &sock->listenners, worker, sock_thread_listenner)) {
        /* Unable to start the worker */
        free(worker);
        return -1;
    }

    return 0;
}

/**
 * @brief Connect a new socket to the wanted host and port
 * @param sock Sock instance
 * @param hostname Hostname
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_connect(sock_t *sock, char *hostname, uint16_t port) {

    assert(NULL != sock);

    /* Create new reader */
    sock_worker_t *worker = (sock_worker_t *)malloc(sizeof(sock_worker_t));
    if (NULL == worker) {
        /* Unable to allocate memory */
        return -1;
    }
    memset(worker, 0, sizeof(sock_worker_t));

    /* Store hostname and port, initialize FDs */
    if (NULL == (worker->type.reader.hostname = strdup(hostname))) {
        /* Unable to allocate memory */
        free(worker);
        return -1;
    }
    worker->type.reader.port = port;
    FD_ZERO(&worker->type.reader.fds);

    /* Start reader */
    if (0 != sock_start_worker(sock, &sock->readers, worker, sock_thread_reader)) {
        /* Unable to start the worker */
        free(worker->type.reader.hostname);
        free(worker);
        return -1;
    }

    return 0;
}

/**
 * @brief Check if sock is already connected to the wanted host and port
 * @param sock Sock instance
 * @param hostname Hostname
 * @param port Port
 * @return true if already connected, false otherwise
 */
bool
sock_is_connected(sock_t *sock, char *hostname, uint16_t port) {

    assert(NULL != sock);
    assert(NULL != hostname);

    bool ret = false;

    /* Parse readers */
    sem_wait(&sock->readers.sem);
    sock_worker_t *worker = sock->readers.first;
    while (NULL != worker) {
        if ((!strcmp(hostname, worker->type.reader.hostname)) && (port == worker->type.reader.port)) {
            ret = true;
            break;
        }
        worker = worker->next;
    }
    sem_post(&sock->readers.sem);

    return ret;
}

/**
 * @brief Register callbacks
 * @param sock Sock instance
 * @param topic Topic
 * @param fct Callback function
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_on(sock_t *sock, char *topic, void *fct, void *user) {

    assert(NULL != sock);
    assert(NULL != topic);

    /* Record callback depending of the topic */
    if (!strcmp(topic, "bind")) {
        sock->cb.bind.fct  = fct;
        sock->cb.bind.user = user;
    } else if (!strcmp(topic, "message")) {
        sock->cb.message.fct  = fct;
        sock->cb.message.user = user;
    } else if (!strcmp(topic, "error")) {
        sock->cb.error.fct  = fct;
        sock->cb.error.user = user;
    }

    return 0;
}

/**
 * @brief Function used to send data
 * @param sock Sock instance
 * @param buffer Buffer to be sent
 * @param size Size of buffer to send
 * @param socket Socket to which the data should be sent, SOCK_SEND_BROADCAST or SOCK_SEND_ROUND_ROBIN
 * @return 0 if the function succeeded, -1 otherwise
 */
int
sock_send(sock_t *sock, void *buffer, size_t size, int socket) {

    assert(NULL != sock);
    assert(NULL != buffer);

    /* Create new sender */
    sock_worker_t *worker = (sock_worker_t *)malloc(sizeof(sock_worker_t));
    if (NULL == worker) {
        /* Unable to allocate memory */
        return -1;
    }
    memset(worker, 0, sizeof(sock_worker_t));

    /* Store buffer, size and socket */
    worker->type.sender.buffer = buffer;
    worker->type.sender.size   = size;
    worker->type.sender.socket = socket;

    /* Start sender */
    if (0 != sock_start_worker(sock, &sock->senders, worker, sock_thread_sender)) {
        /* Unable to start the worker */
        free(worker);
        return -1;
    }

    return 0;
}

/**
 * @brief Release sock instance
 * @param sock Sock instance
 */
void
sock_release(sock_t *sock) {

    /* Release sock instance */
    if (NULL != sock) {

        /* Release listenners */
        sem_wait(&sock->listenners.sem);
        sock_worker_t *worker = sock->listenners.first;
        while (NULL != worker) {
            sock_worker_t *tmp = worker;
            worker             = worker->next;
            pthread_cancel(tmp->thread);
            pthread_join(tmp->thread, NULL);
            for (int index = 0; index < FD_SETSIZE; index++) {
                if (FD_ISSET(index, &tmp->type.listenner.fds)) {
                    close(index);
                }
            }
            free(tmp);
        }
        sem_post(&sock->listenners.sem);
        sem_close(&sock->listenners.sem);

        /* Release readers */
        sem_wait(&sock->readers.sem);
        worker = sock->readers.first;
        while (NULL != worker) {
            sock_worker_t *tmp = worker;
            worker             = worker->next;
            pthread_cancel(tmp->thread);
            pthread_join(tmp->thread, NULL);
            for (int index = 0; index < FD_SETSIZE; index++) {
                if (FD_ISSET(index, &tmp->type.reader.fds)) {
                    close(index);
                }
            }
            free(tmp->type.reader.hostname);
            free(tmp);
        }
        sem_post(&sock->readers.sem);
        sem_close(&sock->readers.sem);

        /* Release messengers */
        sem_wait(&sock->messengers.sem);
        worker = sock->messengers.first;
        while (NULL != worker) {
            sock_worker_t *tmp = worker;
            worker             = worker->next;
            pthread_cancel(tmp->thread);
            pthread_join(tmp->thread, NULL);
            free(tmp->type.messenger.buffer);
            free(tmp);
        }
        sem_post(&sock->messengers.sem);
        sem_close(&sock->messengers.sem);

        /* Release senders */
        sem_wait(&sock->senders.sem);
        worker = sock->senders.first;
        while (NULL != worker) {
            sock_worker_t *tmp = worker;
            worker             = worker->next;
            pthread_cancel(tmp->thread);
            pthread_join(tmp->thread, NULL);
            free(tmp->type.sender.buffer);
            free(tmp);
        }
        sem_post(&sock->senders.sem);
        sem_close(&sock->senders.sem);

        /* Release clients semaphore */
        sem_close(&sock->clients.sem);

        /* Release sock instance */
        free(sock);
    }
}

/**
 * @brief Sock thread used to handle clients connections and reception of data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *
sock_thread_listenner(void *arg) {

    assert(NULL != arg);

    /* Retrieve worker */
    sock_worker_t *worker = (sock_worker_t *)arg;
    sock_t        *sock   = worker->parent;

    /* Create new SOCK_STREAM socket */
    worker->type.listenner.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > worker->type.listenner.socket) {
        /* Unable to create socket */
        if (NULL != sock->cb.error.fct) {
            sock->cb.error.fct(sock, "sock: unable to create listenner socket", sock->cb.error.user);
        }
        goto END;
    }

    /* Add myself to the FDs */
    FD_SET(worker->type.listenner.socket, &worker->type.listenner.fds);

    /* Set socket options */
    int opt = 1;
    if (0 > setsockopt(worker->type.listenner.socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))) {
        /* Unable to set socket option */
        close(worker->type.listenner.socket);
        if (NULL != sock->cb.error.fct) {
            sock->cb.error.fct(sock, "sock: unable to set socket option SO_REUSEADDR", sock->cb.error.user);
        }
        goto END;
    }

    /* Bind socket */
    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(worker->type.listenner.port);
    if (0 > bind(worker->type.listenner.socket, (struct sockaddr *)&addr, sizeof(addr))) {
        /* Unable to bind socket */
        close(worker->type.listenner.socket);
        if (NULL != sock->cb.error.fct) {
            sock->cb.error.fct(sock, "sock: unable to bind socket", sock->cb.error.user);
        }
        goto END;
    }

    /* Invoke bind callback if defined */
    if (NULL != sock->cb.bind.fct) {
        struct sockaddr_in addr_bind;
        size_t             size = sizeof(addr_bind);
        getsockname(worker->type.listenner.socket, (struct sockaddr *)&addr_bind, (socklen_t *)&size);
        uint16_t port = ntohs(addr_bind.sin_port);
        sock->cb.bind.fct(sock, port, sock->cb.bind.user);
    }

    /* Listen for clients */
    if (0 > listen(worker->type.listenner.socket, 1)) {
        /* Unable to listen */
        close(worker->type.listenner.socket);
        if (NULL != sock->cb.error.fct) {
            sock->cb.error.fct(sock, "sock: unable to listen socket", sock->cb.error.user);
        }
        goto END;
    }

    /* Infinite loop */
    while (1) {

        /* Block until input arrives on one or more active sockets */
        fd_set         fds = worker->type.listenner.fds;
        struct timeval tv  = { 5, 0 };
        if (0 > select(FD_SETSIZE, &fds, NULL, NULL, &tv)) {
            /* Unable to select */
        }

        /* Handling of all the sockets with input pending */
        for (int index = 0; index < FD_SETSIZE; index++) {
            if (FD_ISSET(index, &fds)) {
                if (worker->type.listenner.socket == index) {
                    /* Connection request on socket */
                    int                c;
                    struct sockaddr_in addr_client;
                    size_t             size = sizeof(addr_client);
                    if (0 > (c = accept(worker->type.listenner.socket, (struct sockaddr *)&addr_client, (socklen_t *)&size))) {
                        /* Unable to accept the client */
                    } else {
                        /* Add new client to my FDs and parent clients */
                        FD_SET(c, &worker->type.listenner.fds);
                        sem_wait(&sock->clients.sem);
                        FD_SET(c, &sock->clients.fds);
                        sem_post(&sock->clients.sem);
                    }
                } else {
                    /* Data arriving on an already-connected socket */
                    size_t size = 0;
                    ioctl(index, FIONREAD, &size);
                    if (0 < size) {
                        /* Create new messenger */
                        sock_worker_t *w = (sock_worker_t *)malloc(sizeof(sock_worker_t));
                        if (NULL != w) {
                            memset(w, 0, sizeof(sock_worker_t));
                            /* Store socket and size and create buffer */
                            w->type.messenger.socket = index;
                            w->type.messenger.size   = size;
                            w->type.messenger.buffer = malloc(size);
                            if (NULL != w->type.messenger.buffer) {
                                /* Read from socket */
                                if (size == read(index, w->type.messenger.buffer, size)) {
                                    /* Start messenger */
                                    if (0 != sock_start_worker(sock, &sock->messengers, w, sock_thread_messenger)) {
                                        /* Unable to start the worker */
                                        free(w->type.messenger.buffer);
                                        free(w);
                                    }
                                } else {
                                    /* Unable to receive data, close socket */
                                    FD_CLR(index, &worker->type.listenner.fds);
                                    sem_wait(&sock->clients.sem);
                                    FD_CLR(index, &sock->clients.fds);
                                    sem_post(&sock->clients.sem);
                                    close(index);
                                    free(w->type.messenger.buffer);
                                    free(w);
                                }
                            } else {
                                /* Unable to allocate memory */
                                free(w);
                            }
                        }
                    } else {
                        /* Unable to receive data, close socket */
                        FD_CLR(index, &worker->type.listenner.fds);
                        sem_wait(&sock->clients.sem);
                        FD_CLR(index, &sock->clients.fds);
                        sem_post(&sock->clients.sem);
                        close(index);
                    }
                }
            }
        }
    }

    /* Close all clients sockets */
    for (int index = 0; index < FD_SETSIZE; index++) {
        if ((FD_ISSET(index, &worker->type.listenner.fds)) && (worker->type.listenner.socket != index)) {
            close(index);
        }
    }

    /* Close my own socket */
    close(worker->type.listenner.socket);

END:

    /* Remove worker from listenners */
    sock_remove_worker(sock, &sock->listenners, worker);

    /* Release memory */
    free(worker);

    return NULL;
}

/**
 * @brief Sock thread used to handle server connections and reception of data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *
sock_thread_reader(void *arg) {

    assert(NULL != arg);

    /* Retrieve worker */
    sock_worker_t *worker = (sock_worker_t *)arg;
    sock_t        *sock   = worker->parent;

    int  retry     = 100;   /* Connection retry timeout */
    bool connected = false; /* Connection status */

    /* Infinite loop */
    while (1) {

        /* Create new SOCK_STREAM socket */
        worker->type.reader.socket = socket(AF_INET, SOCK_STREAM, 0);
        if (0 > worker->type.reader.socket) {
            /* Unable to create socket */
            retry = (int)(retry * 1.5);
            if (retry > 5000)
                retry = 5000;
            usleep(retry * 1000);
            continue;
        }

        /* Connect to the server */
        struct sockaddr_in addr;
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = inet_addr(worker->type.reader.hostname);
        addr.sin_port        = htons(worker->type.reader.port);
        if (0 > connect(worker->type.reader.socket, (struct sockaddr *)&addr, sizeof(addr))) {
            /* Unable to connect socket */
            retry = (int)(retry * 1.5);
            if (retry > 5000)
                retry = 5000;
            usleep(retry * 1000);
            continue;
        }
        retry     = 100;
        connected = true;

        /* Initialize the set of active sockets, add myself to the set and parent clients */
        FD_ZERO(&worker->type.reader.fds);
        FD_SET(worker->type.reader.socket, &worker->type.reader.fds);
        sem_wait(&sock->clients.sem);
        FD_SET(worker->type.reader.socket, &sock->clients.fds);
        sem_post(&sock->clients.sem);

        /* Loop until disconnection occurs */
        while (true == connected) {

            /* Block until input arrives on one or more active sockets */
            fd_set         fds = worker->type.reader.fds;
            struct timeval tv  = { 5, 0 };
            int            tmp = select(FD_SETSIZE, &fds, NULL, NULL, &tv);
            if (0 > tmp) {
                /* Unable to select */
            }

            /* Handling of all the sockets with input pending */
            for (int index = 0; index < FD_SETSIZE; index++) {
                if ((FD_ISSET(index, &fds)) && (worker->type.reader.socket == index)) {
                    /* Data arriving on an already-connected socket */
                    size_t size = 0;
                    ioctl(index, FIONREAD, &size);
                    if (0 < size) {
                        /* Create new messenger */
                        sock_worker_t *w = (sock_worker_t *)malloc(sizeof(sock_worker_t));
                        if (NULL != w) {
                            memset(w, 0, sizeof(sock_worker_t));
                            /* Store size and create buffer */
                            w->type.messenger.size   = size;
                            w->type.messenger.buffer = malloc(size);
                            if (NULL != w->type.messenger.buffer) {
                                /* Read from socket */
                                if (size == read(index, w->type.messenger.buffer, size)) {
                                    /* Start messenger */
                                    if (0 != sock_start_worker(sock, &sock->messengers, w, sock_thread_messenger)) {
                                        /* Unable to start the worker */
                                        free(w->type.messenger.buffer);
                                        free(w);
                                    }
                                } else {
                                    /* Unable to receive data, close socket */
                                    FD_CLR(index, &worker->type.reader.fds);
                                    sem_wait(&sock->clients.sem);
                                    FD_CLR(index, &sock->clients.fds);
                                    sem_post(&sock->clients.sem);
                                    close(index);
                                    free(w->type.messenger.buffer);
                                    free(w);
                                    connected = false;
                                }
                            } else {
                                /* Unable to allocate memory */
                                free(w);
                            }
                        }
                    } else {
                        /* Unable to receive data, close socket and reconnect again */
                        FD_CLR(index, &worker->type.reader.fds);
                        sem_wait(&sock->clients.sem);
                        FD_CLR(index, &sock->clients.fds);
                        sem_post(&sock->clients.sem);
                        close(index);
                        connected = false;
                    }
                }
            }
        }
    }

    /* Close my own socket */
    sem_wait(&sock->clients.sem);
    FD_CLR(worker->type.reader.socket, &sock->clients.fds);
    sem_post(&sock->clients.sem);
    close(worker->type.reader.socket);

    /* Remove worker from readers */
    sock_remove_worker(sock, &sock->readers, worker);

    /* Release memory */
    free(worker->type.reader.hostname);
    free(worker);

    return NULL;
}

/**
 * @brief Sock thread used to handle data received
 * @param arg Worker
 * @return Always returns NULL
 */
static void *
sock_thread_messenger(void *arg) {

    assert(NULL != arg);

    /* Retrieve worker */
    sock_worker_t *worker = (sock_worker_t *)arg;
    sock_t        *sock   = worker->parent;

    /* Check if message callback is define */
    if (NULL != sock->cb.message.fct) {

        /* Invoke message callback */
        sock->cb.message.fct(sock, worker->type.messenger.buffer, worker->type.messenger.size, worker->type.messenger.socket, sock->cb.message.user);
    }

    /* Remove worker from messengers */
    sock_remove_worker(sock, &sock->messengers, worker);

    /* Release memory */
    free(worker->type.messenger.buffer);
    free(worker);

    return NULL;
}

/**
 * @brief Sock thread used to send data
 * @param arg Worker
 * @return Always returns NULL
 */
static void *
sock_thread_sender(void *arg) {

    assert(NULL != arg);

    /* Retrieve worker */
    sock_worker_t *worker = (sock_worker_t *)arg;
    sock_t        *sock   = worker->parent;

    /* Check wanted destination */
    if (SOCK_SEND_ROUND_ROBIN == worker->type.sender.socket) {

        int socket = -1;
        int retry  = 100;
        int loop   = 0;

        /* Search for the next client socket */
        while (0 > socket) {
            sem_wait(&sock->clients.sem);
            for (int index = 0; index < FD_SETSIZE; index++) {
                int tmp = (index + sock->clients.index + 1) % FD_SETSIZE;
                if (FD_ISSET(tmp, &sock->clients.fds)) {
                    socket = tmp;
                }
            }
            sem_post(&sock->clients.sem);
            if (0 > socket) {
                retry = (int)(retry * 1.5);
                if (retry > 5000) {
                    retry = 5000;
                    loop++;
                    if (3 < loop) {
                        goto END;
                    }
                }
                usleep(retry * 1000);
            }
        }

        /* Client socket found, send data */
        if (worker->type.sender.size != send(socket, worker->type.sender.buffer, worker->type.sender.size, MSG_NOSIGNAL)) {
            /* Unable to send data */
            sem_wait(&sock->clients.sem);
            FD_CLR(socket, &sock->clients.fds);
            close(socket);
            sem_post(&sock->clients.sem);
        }

        /* Increase Round-Robin index */
        sem_wait(&sock->clients.sem);
        sock->clients.index = (sock->clients.index + 1) % FD_SETSIZE;
        sem_post(&sock->clients.sem);

    } else if (SOCK_SEND_BROADCAST == worker->type.sender.socket) {

        /* Send data to all clients sockets */
        sem_wait(&sock->clients.sem);
        for (int index = 0; index < FD_SETSIZE; index++) {
            if ((FD_ISSET(index, &sock->clients.fds))
                && (worker->type.sender.size != send(index, worker->type.sender.buffer, worker->type.sender.size, MSG_NOSIGNAL))) {
                /* Unable to send data */
                FD_CLR(index, &sock->clients.fds);
                close(index);
            }
        }
        sem_post(&sock->clients.sem);

    } else {

        /* Send data to a single socket */
        if (worker->type.sender.size != send(worker->type.sender.socket, worker->type.sender.buffer, worker->type.sender.size, MSG_NOSIGNAL)) {
            /* Unable to send data */
            sem_wait(&sock->clients.sem);
            FD_CLR(worker->type.sender.socket, &sock->clients.fds);
            close(worker->type.sender.socket);
            sem_post(&sock->clients.sem);
        }
    }

END:

    /* Remove worker from senders */
    sock_remove_worker(sock, &sock->senders, worker);

    /* Release memory */
    free(worker->type.sender.buffer);
    free(worker);

    return NULL;
}

/**
 * @brief Start a new worker
 * @param sock Sock instance
 * @param list List of workers to which the worker should be added
 * @param worker Worker to start
 * @param start_routine Worker thread function
 * @return 0 if the function succeeded, -1 otherwise
 */
static int
sock_start_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker, void *(*start_routine)(void *)) {

    /* Wait semaphore */
    sem_wait(&list->sem);

    /* Store sock parent instance */
    worker->parent = sock;

    /* Initialize attributes of the thread */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* Start thread */
    if (0 != pthread_create(&worker->thread, &attr, start_routine, (void *)worker)) {
        /* Unable to start the thread */
        sem_post(&list->sem);
        return -1;
    }

    /* Add worker to the daisy chain */
    if (NULL == list->last) {
        list->first = list->last = worker;
    } else {
        worker->prev     = list->last;
        list->last->next = worker;
        list->last       = worker;
    }

    /* Release semaphore */
    sem_post(&list->sem);

    return 0;
}

/**
 * @brief Remove a worker
 * @param sock Sock instance
 * @param list List of workers to which the worker should be removed
 * @param worker Worker to remove
 * @return 0 if the function succeeded, -1 otherwise
 */
static int
sock_remove_worker(sock_t *sock, sock_worker_list_t *list, sock_worker_t *worker) {

    (void)sock;

    /* Wait semaphore */
    sem_wait(&list->sem);

    /* Remove the worker from the daisy chain */
    if (NULL != worker->prev) {
        worker->prev->next = worker->next;
    } else {
        list->first = worker->next;
    }
    if (NULL != worker->next) {
        worker->next->prev = worker->prev;
    } else {
        list->last = worker->prev;
    }

    /* Release semaphore */
    sem_post(&list->sem);

    return 0;
}
