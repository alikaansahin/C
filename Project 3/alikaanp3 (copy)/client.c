#include "mf.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>


int main(int argc, char *argv[]) {
    if (mf_init() == -1) {
        fprintf(stderr, "Client initialization failed\n");
        return EXIT_FAILURE;
    }
    
    key_t key = ftok("mfserver.c", 'b');  // 'b' is the project identifier
    if (key == -1) {
        perror("ftok");
        return 1;
    }

    printf("Key generated: %x\n", key);

    printf("Client started\n");
    int qid = -1;
    for (int retries = 0; retries < 5 && qid == -1; retries++) {
        qid = mf_open("example_queue");
        printf("%d ", qid);
        if (qid == -1) {
            printf("Attempt %d: Failed to open queue 'example_queue', retrying...\n", retries+1);
            
            sleep(1); // Wait for 1 second before retrying
        }
    }

    qid = msgget(key, 0666);  // Attempt to open the queue
    if (qid == -1) {
	perror("Failed to open message queue");
	fprintf(stderr, "Error code: %d\n", errno);
        return EXIT_FAILURE;
    } else {
        printf("Queue 'example_queue' opened successfully\n");
    }

    // Continue with client operations...

    if (mf_destroy() == -1) {
        fprintf(stderr, "Client cleanup failed\n");
        return EXIT_FAILURE;
    }

    printf("Client cleanup successful.\n");
    return EXIT_SUCCESS;
}

