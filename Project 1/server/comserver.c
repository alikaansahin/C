#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_PIPE_NAME 50
#define MAX_MSG_SIZE 1024
#define MAX_FILENAME 64
#define MAX_CLIENTS 5
#define MAX_COMLINE 256
#define MAXARGS 2
#define MSIZE 1024

typedef struct {
    char id;
    char cs_pipe[MAX_PIPE_NAME];
    char sc_pipe[MAX_PIPE_NAME];
} ConnectionRequest;

typedef struct {
    char msg_size[MAX_MSG_SIZE];
} Message;

enum MessageType {
    CONREQUEST,
    CONREPLY,
    COMLINE,
    COMRESULT,
    QUITREQ,
    QUITREPLY,
    QUITALL
};

void print_message_info(enum MessageType type, const char *data) {
    printf("Message received: type=%d, length=%zu, data=%s\n", type, strlen(data), data);
}

void handle_signal(int signo) {
    if (signo == SIGUSR1) {
        printf("Server received SIGUSR1 signal: Server will end.\n");
        // Handle any cleanup or termination logic
        exit(EXIT_SUCCESS);
    }
}

void handle_command_execution(int cs_fd, const char *command) {
    FILE *output_file = popen(command, "r");
    if (output_file == NULL) {
        perror("popen");
        return;
    }

    char buffer[MAX_MSG_SIZE];
    while (fgets(buffer, MAX_MSG_SIZE, output_file) != NULL) {
        Message result_msg;
        strncpy(result_msg.msg_size, buffer, sizeof(result_msg.msg_size));
        write(cs_fd, &result_msg, sizeof(Message));
    }

    pclose(output_file);
}

void handle_client(mqd_t mq, ConnectionRequest request) {
    int cs_fd = open(request.cs_pipe, O_WRONLY);
    if (cs_fd == -1) {
        perror("open(cs_pipe)");
        return;
    }

    // Sending connection reply
    Message reply_msg;
    snprintf(reply_msg.msg_size, sizeof(reply_msg.msg_size), "Connection established with client %c", request.id);
    write(cs_fd, &reply_msg, sizeof(Message));

    while (1) {
        Message command_msg;
        ssize_t bytes_received = mq_receive(mq, command_msg.msg_size, sizeof(command_msg.msg_size), NULL);

        if (bytes_received <= 0) {
            perror("mq_receive");
            break;
        }

        print_message_info(COMLINE, command_msg.msg_size);

        if (strncmp(command_msg.msg_size, "QUIT", 4) == 0) {
            printf("Received QUIT command. Closing connection.\n");
            break;
        }

        // Implement logic to execute the command and send the result
        handle_command_execution(cs_fd, command_msg.msg_size);
    }

    close(cs_fd);
}

void handle_connection_requests(mqd_t mq) {
    ConnectionRequest request;

    while (1) {
        printf("Waiting for connection request...\n");

        char *buffer;
        size_t bufferSize = sizeof(ConnectionRequest) + 1;  // Add extra byte for safety

        buffer = (char *)malloc(bufferSize);

        ssize_t bytes_received = mq_receive(mq, buffer, bufferSize, NULL);

        if (bytes_received == -1) {
            perror("mq_receive");
            free(buffer);
            continue;
        }

        print_message_info(CONREQUEST, buffer);

        // Validate the received message size
        if ((size_t)bytes_received != bufferSize) {
            fprintf(stderr, "Received message size does not match the expected size.\n");
            free(buffer);
            continue;
        }

        memcpy(&request, buffer, sizeof(ConnectionRequest));
        free(buffer);

        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            continue;
        }

        if (child_pid == 0) {
            // Child process (server child)
            handle_client(mq, request);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process (main server)
            wait(NULL); // Wait for the child process to finish
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s MQNAME\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    mqd_t mq = mq_open(argv[1], O_CREAT | O_RDWR, 0666, NULL);
    if (mq == -1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    // Register signal handler for SIGUSR1
    signal(SIGUSR1, handle_signal);

    handle_connection_requests(mq);

    mq_close(mq);
    mq_unlink(argv[1]);

    return 0;
}