#include "mf.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <errno.h>
#include <sys/msg.h>

volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    keep_running = 0; // Signal the main loop to exit
}

int setup_signals() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_signal); // Handle Ctrl+C
    signal(SIGTERM, handle_signal); // Handle terminate signal
    
    if (setup_signals() == -1) {
        exit(EXIT_FAILURE);
    }
    
    key_t key = ftok("client.c", 'b');  // Ensure this file exists
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }

    printf("Message queue created with ID: %d\n", msgid);

    // Check queue status
    struct msqid_ds buf;
    if (msgctl(msgid, IPC_STAT, &buf) == -1) {
        perror("msgctl IPC_STAT failed");
        exit(EXIT_FAILURE);
    }

    printf("Queue size: %lu\n", buf.msg_qbytes);
    printf("Number of messages: %lu\n", buf.msg_qnum);

    
    if (mf_init() == -1) {
        fprintf(stderr, "Initialization failed\n");
        return EXIT_FAILURE;
    }

    printf("Server running with PID=%d\n", getpid());
    int qid = mf_create("example_queue", 1024); // Create a queue to post messages
    

    while (keep_running) {
        if (mf_send(qid, "Hello from server", 17) == -1) {
            fprintf(stderr, "Failed to send message\n");
        } else {
            printf("Message sent successfully\n");
        }
        sleep(1); // Wait a second before sending another message
    }

    if (mf_destroy() == -1) {
        fprintf(stderr, "Cleanup failed\n");
        return EXIT_FAILURE;
    }
    
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID failed");
        exit(EXIT_FAILURE);
    }

    printf("Server cleanup successful.\n");
    return EXIT_SUCCESS;
}

