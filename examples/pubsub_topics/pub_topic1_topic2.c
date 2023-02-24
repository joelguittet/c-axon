/**
 * @file      pub_topic1_topic2.c
 * @brief     Axon Pub example in C
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
#include <unistd.h>
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

    axon_t *axon;

    /* Initialize sig handler */
    signal(SIGINT, sig_handler);

    /* Create Axon "pub" instance and bind on port 3000 */
    if (NULL == (axon = axon_create("pub"))) {
        printf("unable to create axon instance\n");
        exit(EXIT_FAILURE);
    }
    if (0 != axon_bind(axon, 3000)) {
        printf("unable to bind axon instance\n");
        exit(EXIT_FAILURE);
    }

    printf("pub server started\n");

    /* Loop */
    while (false == terminate) {

        printf("sending\n");

        /* Sending to topic1 */
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "payload", "the payload of topic 1");
        axon_send(axon, 2, AMP_TYPE_STRING, "topic1", AMP_TYPE_JSON, json);
        cJSON_Delete(json);

        /* Sending to topic2 */
        json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "payload", "the payload of topic 2");
        axon_send(axon, 2, AMP_TYPE_STRING, "topic2", AMP_TYPE_JSON, json);
        cJSON_Delete(json);

        /* Wait for a while */
        sleep(1);
    }

    /* Release memory */
    axon_release(axon);

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
