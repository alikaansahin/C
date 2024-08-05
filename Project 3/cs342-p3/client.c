#include "mf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    if (mf_connect() == -1) {
        fprintf(stderr, "Failed to connect to the message facility\n");
        return EXIT_FAILURE;
    }

    int qid;
    char *queue_name = "test_queue";
    if ((qid = mf_create(queue_name, 1024)) == -1) {
        fprintf(stderr, "Failed to create message queue\n");
        return EXIT_FAILURE;
    }

    char message[] = "Hello, World!";
    if (mf_send(qid, message, sizeof(message)) == -1) {
        fprintf(stderr, "Failed to send message\n");
        return EXIT_FAILURE;
    }

    char buffer[1024];
    if (mf_recv(qid, buffer, sizeof(buffer)) == -1) {
        fprintf(stderr, "Failed to receive message\n");
        return EXIT_FAILURE;
    }

    printf("Received message: %s\n", buffer);

    if (mf_remove(queue_name) == -1) {
        fprintf(stderr, "Failed to remove message queue\n");
        return EXIT_FAILURE;
    }

    if (mf_disconnect() == -1) {
        fprintf(stderr, "Failed to disconnect\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
