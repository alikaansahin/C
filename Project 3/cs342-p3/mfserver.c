#include "mf.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    keep_running = 0; // Signal the main loop to exit
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_signal); // Handle Ctrl+C
    signal(SIGTERM, handle_signal); // Handle terminate signal

    if (mf_init() == -1) {
        fprintf(stderr, "Initialization failed\n");
        return EXIT_FAILURE;
    }

    printf("Server running with PID=%d\n", getpid());

    while (keep_running) {
        pause();  // Wait for a signal
    }

    if (mf_destroy() == -1) {
        fprintf(stderr, "Cleanup failed\n");
        return EXIT_FAILURE;
    }

    printf("Server cleanup successful.\n");
    return EXIT_SUCCESS;
}