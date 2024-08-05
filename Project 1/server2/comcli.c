#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <mqueue.h>

#define MAXFILENAME 64
#define MAXCLIENTS 5
#define MAXCOMLINE 256
#define MAXARGS 5

struct Message {
    unsigned int length;
    unsigned char type;
    unsigned char padding[3];
    char data[1024];
};

#define CONREQUEST 1
#define CONREPLY 2
#define COMLINE 3
#define COMRESULT 4
#define QUITREQ 5
#define QUITREPLY 6
#define QUITALL 7

void handle_connection_reply(mqd_t mq, int sc_pipe_fd, struct Message *msg);
void handle_command_result(int sc_pipe_fd, struct Message *msg);

mqd_t client_mq;

void cleanup() {
    mq_close(client_mq);
    exit(EXIT_SUCCESS);
}

int main() {
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct Message) + MAXCOMLINE;

    client_mq = mq_open("/comserver", O_RDWR);

    if (client_mq == (mqd_t) - 1) {
        perror("Client: mq_open");
        exit(EXIT_FAILURE);
    }

    int sc_pipe[2];

    if (pipe(sc_pipe) == -1) {
        perror("pipe");
        cleanup();
    }

    // Your main client loop here
    while (1) {

        struct Message *msg = malloc(sizeof(struct Message) + MAXCOMLINE);

        // Set up your message and send it to the server
        // Example:
        msg->type = COMLINE;
        // Fill in the data field with your command line

        // Send the message to the server
        mq_send(client_mq, (const char *)msg, msg->length, 0);

        // Receive the result from the server
        mq_receive(client_mq, (char *)msg, attr.mq_msgsize, NULL);

        // Handle the result
        handle_command_result(sc_pipe[0], msg);

        free(msg);
    }

    close(sc_pipe[0]);
    close(sc_pipe[1]);

    cleanup();
}

void handle_connection_reply(mqd_t mq, int sc_pipe_fd, struct Message *msg) {
    // Handle connection reply logic
    // You might want to set up pipes for communication with the server
    // Create child process for command execution, pass arguments via pipes, and wait for result
    // Example:
    // Extract the queue name from the message and open it
    mqd_t client_queue = mq_open(msg->data, O_WRONLY);

    // Set up pipes for communication with the child process
    int pipes[2];
    pipe(pipes);

    pid_t child_pid = fork();

    if (child_pid == 0) {
        printf("deneme");
        // Child process
        // Redirect standard output to the write end of the pipe
        close(pipes[0]);
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[1]);

        // Execute the command line interpreter or desired logic
        // Example:
        execlp("cominterpreter", "cominterpreter", (char *)NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else if (child_pid > 0) {
        // Parent process
        // Close the write end of the pipe
        close(pipes[1]);

        // Send a COMLINE message to the server with the command line
        struct Message comline_msg;
        comline_msg.type = COMLINE;
        // Fill in the data field with the command line
        mq_send(mq, (const char *)&comline_msg, comline_msg.length, 0);

        // Read the result from the pipe and send it back to the server
        char buffer[MAXCOMLINE];
        ssize_t bytes_read = read(pipes[0], buffer, sizeof(buffer));
        close(pipes[0]);

        // Send a COMRESULT message to the server with the result
        struct Message comresult_msg;
        comresult_msg.type = COMRESULT;
        // Fill in the data field with the result
        mq_send(mq, (const char *)&comresult_msg, comresult_msg.length, 0);
    } else {
        perror("fork");
    }
}

void handle_command_result(int sc_pipe_fd, struct Message *msg) {
    // Handle command result logic
    // Display the result to the user
    // Example:
    printf("Result: %s\n", msg->data);
}
