/**
 * @file pub.c
 * @brief Axon Pub example in C
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

static bool terminate = false;                                      /* Flag used to terminate the application */


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
int main(int argc, char** argv) {

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
static void sig_handler(int signo) {

  /* SIGINT handling */
  if (SIGINT == signo) {
    terminate = true;
  }
}
