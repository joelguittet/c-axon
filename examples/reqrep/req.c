/**
 * @file      req.c
 * @brief     Axon Req example in C
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
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <signal.h>
#include <cJSON.h>

#include "axon.h"

/******************************************************************************/
/* Variables                                                                  */
/******************************************************************************/

static bool terminate = false; /* Flag used to terminate the application */

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/

/**
 * @brief Signal hanlder
 * @param signo Signal number
 */
static void sig_handler(int signo);

/******************************************************************************/
/* Functions                                                                  */
/******************************************************************************/

/**
 * @brief Main function
 * @param argc Number of arguments
 * @param argv Arguments
 * @return Always returns 0
 */
int
main(int argc, char **argv) {

    axon_t *sock;

    /* Initialize sig handler */
    signal(SIGINT, sig_handler);

    /* Create Axon "req" instance and connect on port 3000 */
    if (NULL == (sock = axon_create("req"))) {
        printf("unable to create axon instance\n");
        exit(EXIT_FAILURE);
    }
    if (0 != axon_connect(sock, "127.0.0.1", 3000)) {
        printf("unable to connect axon instance\n");
        exit(EXIT_FAILURE);
    }

    printf("req client started\n");

    /* Loop */
    while (false == terminate) {

        printf("sending\n");

        /* Sending JSON object and retrieve response */
        amp_msg_t *amp  = NULL;
        cJSON     *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "hello", "world");
        if (0 == axon_send(sock, 1, AMP_TYPE_JSON, json, &amp, 5000)) {

            assert(NULL != amp);

            int64_t bint;
            char   *str;

            printf("req client message received\n");

            /* Parse all fields of the message */
            amp_field_t *field = amp_get_first(amp);
            while (NULL != field) {

                /* Switch depending of the type */
                switch (field->type) {
                    case AMP_TYPE_BLOB:
                        printf("<Buffer");
                        for (int index_data = 0; index_data < field->size; index_data++) {
                            printf(" %02x", ((unsigned char *)field->data)[index_data]);
                        }
                        printf(">\n");
                        break;
                    case AMP_TYPE_STRING:
                        printf("%s\n", (char *)field->data);
                        break;
                    case AMP_TYPE_BIGINT:
                        bint = (*(int64_t *)field->data);
                        printf("%" PRId64 "\n", bint);
                        break;
                    case AMP_TYPE_JSON:
                        str = cJSON_PrintUnformatted((cJSON *)field->data);
                        printf("%s\n", str);
                        free(str);
                        break;
                    default:
                        /* Should not occur */
                        break;
                }

                /* Next field */
                field = amp_get_next(amp);
            }

            /* Release memory */
            amp_release(amp);
        }

        /* Release memory */
        cJSON_Delete(json);

        /* Wait for a while */
        sleep(1);
    }

    /* Release memory */
    axon_release(sock);

    return 0;
}

/**
 * @brief Signal hanlder
 * @param signo Signal number
 */
static void
sig_handler(int signo) {

    /* SIGINT handling */
    if (SIGINT == signo) {
        terminate = true;
    }
}
