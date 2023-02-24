/**
 * @file      axon.c
 * @brief     Axon library
 *
 * MIT License
 *
 * Copyright (c) 2021-2023 joelguittet and c-axon contributors
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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <mqueue.h>
#include <regex.h>
#include <cJSON.h>
#include <time.h>

#include "axon.h"
#include "sock.h"

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Callback function called when socket is bound
 * @param sock Sock instance
 * @param port Port on which the socket is bound
 * @param user User data
 */
static void axon_bind_cb(sock_t *sock, uint16_t port, void *user);

/**
 * @brief Callback function called to handle received data
 * @param sock Sock instance
 * @param buffer Data received
 * @param size Size of data received
 * @param socket Socket from which the data are received
 * @param user User data
 */
static void axon_message_cb(sock_t *sock, void *buffer, size_t size, int socket, void *user);

/**
 * @brief Callback function called to handle error from sock instance
 * @param sock Sock instance
 * @param err Error as string
 * @param user User data
 */
static void axon_error_cb(sock_t *sock, char *err, void *user);

/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/

/**
 * @brief Function used to create axon instance
 * @param type Type of Axon instance
 * @return Axon instance if the function succeeded, NULL otherwise
 */
axon_t *
axon_create(char *type) {

    assert(NULL != type);

    /* Create axon instance */
    axon_t *axon = (axon_t *)malloc(sizeof(axon_t));
    if (NULL == axon) {
        /* Unable to allocate memory */
        return NULL;
    }
    memset(axon, 0, sizeof(axon_t));

    /* Set Axon type */
    if (!strcmp(type, "pub")) {
        axon->type = AXON_TYPE_PUB;
    } else if (!strcmp(type, "sub")) {
        axon->type = AXON_TYPE_SUB;
    } else if (!strcmp(type, "push")) {
        axon->type = AXON_TYPE_PUSH;
    } else if (!strcmp(type, "pull")) {
        axon->type = AXON_TYPE_PULL;
    } else if (!strcmp(type, "req")) {
        axon->type = AXON_TYPE_REQ;
    } else if (!strcmp(type, "rep")) {
        axon->type = AXON_TYPE_REP;
    } else {
        /* Incorrect type */
        free(axon);
        return NULL;
    }

    /* Create sock instance */
    if (NULL == (axon->sock = sock_create())) {
        /* Unable to allocate memory */
        free(axon);
        return NULL;
    }

    /* Initialize semaphore used to access subscriptions */
    sem_init(&axon->subs.sem, 0, 1);

    /* Register message and error callbacks */
    sock_on(axon->sock, "bind", &axon_bind_cb, axon);
    if ((AXON_TYPE_SUB == axon->type) || (AXON_TYPE_PULL == axon->type) || (AXON_TYPE_REQ == axon->type) || (AXON_TYPE_REP == axon->type)) {
        sock_on(axon->sock, "message", &axon_message_cb, axon);
    }
    sock_on(axon->sock, "error", &axon_error_cb, axon);

    return axon;
}

/**
 * @brief Bind axon on the wanted port
 * @param axon Axon instance
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
int
axon_bind(axon_t *axon, uint16_t port) {

    assert(NULL != axon);
    assert(NULL != axon->sock);

    return sock_bind(axon->sock, port);
}

/**
 * @brief Connect axon to the wanted host and port
 * @param axon Axon instance
 * @param hostname Hostname
 * @param port Port
 * @return 0 if the function succeeded, -1 otherwise
 */
int
axon_connect(axon_t *axon, char *hostname, uint16_t port) {

    assert(NULL != axon);
    assert(NULL != axon->sock);
    assert(NULL != hostname);

    return sock_connect(axon->sock, hostname, port);
}

/**
 * @brief Check if axon is already connected to the wanted host and port
 * @param axon Axon instance
 * @param hostname Hostname
 * @param port Port
 * @return true if already connected, false otherwise
 */
bool
axon_is_connected(axon_t *axon, char *hostname, uint16_t port) {

    assert(NULL != axon);
    assert(NULL != axon->sock);
    assert(NULL != hostname);

    return sock_is_connected(axon->sock, hostname, port);
}

/**
 * @brief Register callbacks
 * @param axon Axon instance
 * @param topic Topic
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int
axon_on(axon_t *axon, char *topic, void *fct, void *user) {

    assert(NULL != axon);
    assert(NULL != topic);

    /* Record callback depending of the topic */
    if (!strcmp(topic, "bind")) {
        axon->cb.bind.fct  = fct;
        axon->cb.bind.user = user;
    } else if (!strcmp(topic, "message")) {
        axon->cb.message.fct  = fct;
        axon->cb.message.user = user;
    } else if (!strcmp(topic, "error")) {
        axon->cb.error.fct  = fct;
        axon->cb.error.user = user;
    }

    return 0;
}

/**
 * @brief Subscribe to wanted topic
 * @param axon Axon instance
 * @param topic Topic
 * @param fct Callback funtion
 * @param user User data
 * @return 0 if the function succeeded, -1 otherwise
 */
int
axon_subscribe(axon_t *axon, char *topic, void *fct, void *user) {

    assert(NULL != axon);
    assert(NULL != topic);

    /* Check Axon instance type */
    if ((AXON_TYPE_SUB != axon->type) && (AXON_TYPE_PULL != axon->type)) {
        /* Not compatible */
        return -1;
    }

    int         ret      = 0;
    axon_sub_t *last_sub = NULL;

    /* Wait semaphore */
    sem_wait(&axon->subs.sem);

    /* Parse subscriptions, update callback and user data if topic is found */
    axon_sub_t *curr_sub = axon->subs.first;
    while (NULL != curr_sub) {
        if (!strcmp(topic, curr_sub->topic)) {
            curr_sub->fct  = fct;
            curr_sub->user = user;
            goto LEAVE;
        }
        last_sub = curr_sub;
        curr_sub = curr_sub->next;
    }

    /* Subscription not found, add a new one */
    axon_sub_t *new_sub = (axon_sub_t *)malloc(sizeof(axon_sub_t));
    if (NULL == new_sub) {
        /* Unable to allocate memory */
        ret = -1;
        goto LEAVE;
    }
    memset(new_sub, 0, sizeof(axon_sub_t));
    new_sub->topic = strdup(topic);
    if (NULL == new_sub->topic) {
        /* Unable to allocate memory */
        free(new_sub);
        ret = -1;
        goto LEAVE;
    }
    new_sub->fct  = fct;
    new_sub->user = user;
    if (NULL != last_sub) {
        last_sub->next = new_sub;
    } else {
        axon->subs.first = new_sub;
    }

LEAVE:

    /* Release semaphore */
    sem_post(&axon->subs.sem);

    return ret;
}

/**
 * @brief Unubscribe to wanted topic
 * @param axon Axon instance
 * @param topic Topic
 * @return 0 if the function succeeded, -1 otherwise
 */
int
axon_unsubscribe(axon_t *axon, char *topic) {

    assert(NULL != axon);
    assert(NULL != topic);

    /* Check Axon instance type */
    if ((AXON_TYPE_SUB != axon->type) && (AXON_TYPE_PULL != axon->type)) {
        /* Not compatible */
        return -1;
    }

    axon_sub_t *last_sub = NULL;

    /* Wait semaphore */
    sem_wait(&axon->subs.sem);

    /* Parse subscriptions, remove subscription if topic is found */
    axon_sub_t *curr_sub = axon->subs.first;
    while (NULL != curr_sub) {
        if (!strcmp(topic, curr_sub->topic)) {
            if (axon->subs.first == curr_sub) {
                axon->subs.first = curr_sub->next;
            } else {
                last_sub->next = curr_sub->next;
            }
            free(curr_sub->topic);
            free(curr_sub);
            goto LEAVE;
        }
        last_sub = curr_sub;
        curr_sub = curr_sub->next;
    }

LEAVE:

    /* Release semaphore */
    sem_post(&axon->subs.sem);

    return 0;
}

/**
 * @brief Function used to send data to the server or to all connected clients
 * @param axon Axon instance
 * @param count Amount of data to be sent
 * @param type1 Type of the first field of the message
 * @param value1 Value  of the first field of the message
 * @param ... type, data Array of type and data (and size for blob type) to be sent, AMP response message and timeout for Requester instance
 * @return 0 if the function succeeded, -1 otherwise
 */
int
axon_send(axon_t *axon, int count, amp_type_e type1, void *value1, ...) {

    /* Retrieve params */
    va_list params;
    va_start(params, value1);

    /* Send message */
    int ret = axon_vsend(axon, count, type1, value1, params);

    /* End of params */
    va_end(params);

    return ret;
}

/**
 * @brief Function used to send data to the server or to all connected clients
 * @param axon Axon instance
 * @param count Amount of data to be sent
 * @param type1 Type of the first field of the message
 * @param value1 Value  of the first field of the message
 * @param params type, data Array of type and data (and size for blob type) to be sent, AMP response message and timeout for Requester instance
 * @return 0 if the function succeeded, -1 otherwise
 */
int
axon_vsend(axon_t *axon, int count, amp_type_e type1, void *value1, va_list params) {

    void *      blob   = NULL;
    size_t      size   = 0;
    char *      str    = NULL;
    int64_t     bint   = 0;
    cJSON *     json   = NULL;
    void *      buffer = NULL;
    char        str_id[32 + 1];
    amp_msg_t **resp    = NULL;
    int         timeout = 0;
    char        str_mq[64 + 1];
    mqd_t       mq;

    assert(NULL != axon);
    assert(NULL != axon->sock);

    /* Check Axon instance type */
    if ((AXON_TYPE_PUB != axon->type) && (AXON_TYPE_PUSH != axon->type) && (AXON_TYPE_REQ != axon->type)) {
        /* Not compatible */
        return -1;
    }

    /* Create new AMP message */
    amp_msg_t *amp = amp_create();
    if (NULL == amp) {
        /* Unable to allocate memory */
        return -1;
    }

    /* Push params to AMP message */
    switch (type1) {
        case AMP_TYPE_BLOB:
            blob = value1;
            size = va_arg(params, int);
            amp_push(amp, type1, blob, size);
            break;
        case AMP_TYPE_STRING:
            str = (char *)value1;
            amp_push(amp, type1, str);
            break;
        case AMP_TYPE_BIGINT:
            bint = (int64_t)value1;
            amp_push(amp, type1, bint);
            break;
        case AMP_TYPE_JSON:
            json = (cJSON *)value1;
            amp_push(amp, type1, json);
            break;
        default:
            /* Should not occur */
            break;
    }
    for (int index = 1; index < count; index++) {
        amp_type_e type = va_arg(params, int);
        switch (type) {
            case AMP_TYPE_BLOB:
                blob = va_arg(params, void *);
                size = va_arg(params, int);
                amp_push(amp, type, blob, size);
                break;
            case AMP_TYPE_STRING:
                str = va_arg(params, char *);
                amp_push(amp, type, str);
                break;
            case AMP_TYPE_BIGINT:
                bint = va_arg(params, int64_t);
                amp_push(amp, type, bint);
                break;
            case AMP_TYPE_JSON:
                json = va_arg(params, cJSON *);
                amp_push(amp, type, json);
                break;
            default:
                /* Should not occur */
                break;
        }
    }

    /* If Axon instance is Requester, retrieve AMP response address and timeout */
    if (AXON_TYPE_REQ == axon->type) {
        resp    = va_arg(params, amp_msg_t **);
        timeout = va_arg(params, int);
    }

    /* If Axon instance is Requester, push the id field at the end of the message */
    if (AXON_TYPE_REQ == axon->type) {

        /* Create the message ID */
        snprintf(str_id, 32, "%d:%u", getpid(), axon->msg_id);
        axon->msg_id++;

        /* Push id at the end of the message */
        amp_push(amp, AMP_TYPE_STRING, str_id, strlen(str_id));
    }

    /* Encode AMP message */
    if (0 != amp_encode(amp, &buffer, &size)) {
        /* Unable to encode message */
        amp_release(amp);
        return -1;
    }

    /* Release memory */
    amp_release(amp);

    /* If Axon instance is Requester, create a new message queue to retrieve the response */
    if (AXON_TYPE_REQ == axon->type) {
        snprintf(str_mq, 64, "/%s", str_id);
        struct mq_attr attr = { 0, 1, sizeof(amp_msg_t *), 0 };
        if (0 > (mq = mq_open(str_mq, O_CREAT | O_RDONLY, 0644, &attr))) {
            /* Unable to create message queue */
            free(buffer);
            return -1;
        }
    }

    /* Send AMP encoded buffer */
    if (0 != sock_send(axon->sock, buffer, size, (AXON_TYPE_PUB == axon->type) ? SOCK_SEND_BROADCAST : SOCK_SEND_ROUND_ROBIN)) {
        /* Unable to send data */
        if (AXON_TYPE_REQ == axon->type) {
            mq_close(mq);
            mq_unlink(str_mq);
        }
        free(buffer);
        return -1;
    }

    /* If Axon instance is Requester, wait for the response */
    if (AXON_TYPE_REQ == axon->type) {
        struct timespec tm;
        clock_gettime(CLOCK_REALTIME, &tm);
        tm.tv_sec += timeout / 1000;
        amp_msg_t *tmp = NULL;
        if (0 > mq_timedreceive(mq, (char *)&tmp, sizeof(amp_msg_t *), NULL, &tm)) {
            /* Unable to receive data */
            mq_close(mq);
            mq_unlink(str_mq);
            return -1;
        }
        mq_close(mq);
        mq_unlink(str_mq);
        *resp = tmp;
    }

    return 0;
}

/**
 * @brief Function used by Replier instance to format response to the server or to a single client
 * @param axon Axon instance
 * @param count Amount of data to be sent
 * @param ... type, data Array of type and data (and size for blob type) to be sent
 * @return AMP response message
 */
amp_msg_t *
axon_reply(axon_t *axon, int count, ...) {

    void *  blob = NULL;
    size_t  size = 0;
    char *  str  = NULL;
    int64_t bint = 0;
    cJSON * json = NULL;

    assert(NULL != axon);

    /* Check Axon instance type */
    if (AXON_TYPE_REP != axon->type) {
        /* Not compatible */
        return NULL;
    }

    /* Create new AMP message */
    amp_msg_t *amp = amp_create();
    if (NULL == amp) {
        /* Unable to allocate memory */
        return NULL;
    }

    /* Push params to AMP message */
    va_list params;
    va_start(params, count);
    for (int index = 0; index < count; index++) {
        amp_type_e type = va_arg(params, int);
        switch (type) {
            case AMP_TYPE_BLOB:
                blob = va_arg(params, void *);
                size = va_arg(params, int);
                amp_push(amp, type, blob, size);
                break;
            case AMP_TYPE_STRING:
                str = va_arg(params, char *);
                amp_push(amp, type, str);
                break;
            case AMP_TYPE_BIGINT:
                bint = va_arg(params, int64_t);
                amp_push(amp, type, bint);
                break;
            case AMP_TYPE_JSON:
                json = va_arg(params, cJSON *);
                amp_push(amp, type, json);
                break;
            default:
                /* Should not occur */
                break;
        }
    }
    va_end(params);

    return amp;
}

/**
 * @brief Release axon instance
 * @param axon Axon instance
 */
void
axon_release(axon_t *axon) {

    /* Release axon instance */
    if (NULL != axon) {

        /* Release sock instance */
        sock_release(axon->sock);

        /* Release subscriptions */
        sem_wait(&axon->subs.sem);
        axon_sub_t *curr_sub = axon->subs.first;
        while (NULL != curr_sub) {
            axon_sub_t *tmp = curr_sub;
            curr_sub        = curr_sub->next;
            if (NULL != tmp->topic) {
                free(tmp->topic);
            }
            free(tmp);
        }
        sem_post(&axon->subs.sem);
        sem_close(&axon->subs.sem);

        /* Release Axon instance */
        free(axon);
    }
}

/**
 * @brief Callback function called when socket is bound
 * @param sock Sock instance
 * @param port Port on which the socket is bound
 * @param user User data
 */
static void
axon_bind_cb(sock_t *sock, uint16_t port, void *user) {

    (void)sock;
    assert(NULL != user);

    /* Retrieve axon instance using user data */
    axon_t *axon = (axon_t *)user;

    /* Invoke bind callback if defined */
    if (NULL != axon->cb.bind.fct) {
        axon->cb.bind.fct(axon, port, axon->cb.bind.user);
    }
}

/**
 * @brief Callback function called to handle received data
 * @param sock Sock instance
 * @param buffer Data received
 * @param size Size of data received
 * @param socket Socket from which the data are received
 * @param user User data
 */
static void
axon_message_cb(sock_t *sock, void *buffer, size_t size, int socket, void *user) {

    (void)sock;
    assert(NULL != buffer);
    assert(NULL != user);

    /* Retrieve axon instance using user data */
    axon_t *axon = (axon_t *)user;

    /* Because multiple messages can be received once (but always from the same socket), parse until all the buffer is decoded */
    while (0 < size) {

        /* Create new AMP message */
        amp_msg_t *amp = amp_create();
        if (NULL == amp) {
            /* Unable to allocate memory */
            return;
        }

        /* Decode AMP message */
        if (0 != amp_decode(amp, &buffer, &size)) {
            /* Unable to encode message */
            amp_release(amp);
            return;
        }

        /* Check the message has at least one field */
        if ((NULL == amp->first) || (NULL == amp->last)) {
            /* Invalid message */
            amp_release(amp);
            return;
        }

        /* Treatment depending of Axon instance type */
        if (AXON_TYPE_REQ == axon->type) {

            /* Axon is Requester, remove the ID field of the response at the end of the message */
            amp_field_t *id_field = amp->last;
            if (NULL != amp->last->prev) {
                amp->last       = amp->last->prev;
                amp->last->next = NULL;
            }
            amp->count--;

            /* Open message queue and send the response - If this fails maybe this is because timeout elapsed (ignored) */
            mqd_t mq;
            char  str_mq[64 + 1];
            snprintf(str_mq, 64, "/%s", (char *)id_field->data);
            if (0 > (mq = mq_open(str_mq, O_WRONLY))) {
                /* Unable to open messqge queue (ignored) */
                amp_release(amp);
            } else {
                if (0 > mq_send(mq, (const char *)&amp, sizeof(amp_msg_t *), 0)) {
                    /* Unable to send data (ignored) */
                    amp_release(amp);
                }
                mq_close(mq);
            }

            /* Release memory */
            free(id_field->data);
            free(id_field);

        } else if (AXON_TYPE_REP == axon->type) {

            /* Axon is Replier, remove the ID field of the request at the end of the message */
            amp_field_t *id_field = amp->last;
            if (NULL != amp->last->prev) {
                amp->last       = amp->last->prev;
                amp->last->next = NULL;
            }
            amp->count--;

            /* Check if message callback is define */
            if (NULL != axon->cb.message.fct) {

                /* Invoke message callback */
                amp_msg_t *rep = axon->cb.message.fct(axon, amp, axon->cb.message.user);

                /* Check if reply is provided */
                if (NULL != rep) {

                    /* Push the id field of the request at the end of the response */
                    amp_push(rep, id_field->type, id_field->data, id_field->size);

                    /* Encode AMP message */
                    void * buffer_rep = NULL;
                    size_t size_rep   = 0;
                    if (0 != amp_encode(rep, &buffer_rep, &size_rep)) {
                        /* Unable to encode message */
                    } else {
                        /* Send AMP encoded buffer */
                        if (0 != sock_send(axon->sock, buffer_rep, size_rep, socket)) {
                            /* Unable to send data */
                        }
                    }

                    /* Release memory */
                    amp_release(rep);
                }
            }

            /* Release memory */
            amp_release(amp);
            free(id_field->data);
            free(id_field);

        } else {

            /* Axon is Subscriber or Puller */

            /* Check if message callback is define */
            if (NULL != axon->cb.message.fct) {

                /* Invoke message callback */
                axon->cb.message.fct(axon, amp, axon->cb.message.user);
            }

            /* Wait subscriptions semaphore */
            sem_wait(&axon->subs.sem);

            /* Invoke susbscriptions callback(s) if defined and if the first field of the AMP message is a string */
            if ((NULL != axon->subs.first) && (AMP_TYPE_STRING == amp->first->type) && (NULL != amp->first->data)) {

                /* Extract topic from the message */
                amp_field_t *topic_field = amp->first;
                amp->first               = amp->first->next;
                amp->count--;

                /* Parse all subscriptions */
                axon_sub_t *curr_sub = axon->subs.first;
                while (NULL != curr_sub) {
                    if (NULL != curr_sub->fct) {
                        regex_t regex;
                        if (0 == regcomp(&regex, curr_sub->topic, REG_NOSUB | REG_EXTENDED)) {
                            if (0 == regexec(&regex, topic_field->data, 0, NULL, 0)) {

                                /* Topic match subscription */

                                /* Invoke subscription callback */
                                curr_sub->fct(axon, topic_field->data, amp, curr_sub->user);
                            }
                            regfree(&regex);
                        }
                    }
                    curr_sub = curr_sub->next;
                }
                free(topic_field->data);
                free(topic_field);
            }

            /* Release subscriptions semaphore */
            sem_post(&axon->subs.sem);

            /* Release memory */
            amp_release(amp);
        }
    }
}

/**
 * @brief Callback function called to handle error from sock instance
 * @param sock Sock instance
 * @param err Error as string
 * @param user User data
 */
static void
axon_error_cb(sock_t *sock, char *err, void *user) {

    (void)sock;
    assert(NULL != err);
    assert(NULL != user);

    /* Retrieve axon instance using user data */
    axon_t *axon = (axon_t *)user;

    /* Invoke error callback if defined */
    if (NULL != axon->cb.error.fct) {
        axon->cb.error.fct(axon, err, axon->cb.error.user);
    }
}
