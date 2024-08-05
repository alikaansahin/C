#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>

#define MAX_PIPE_NAME 50
#define MAX_MSG_SIZE 1024
#define MAX_WSIZE 8196

typedef struct {
    char id;
    char cs_pipe[MAX_PIPE_NAME];
    char sc_pipe[MAX_PIPE_NAME];
    int wsize;
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

int interactive_mode(int cs_fd, int sc_fd, mqd_t mq);
int batch_mode(int cs_fd, int sc_fd, const char *comfile);

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 5) {
        fprintf(stderr, "Usage: %s MQNAME [-b COMFILE] [-s WSIZE]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char cs_pipe[MAX_PIPE_NAME];
    char sc_pipe[MAX_PIPE_NAME];
    snprintf(cs_pipe, sizeof(cs_pipe), "/tmp/cs_pipe_%d", getpid());
    snprintf(sc_pipe, sizeof(sc_pipe), "/tmp/sc_pipe_%d", getpid());

    mkfifo(cs_pipe, 0666);
    mkfifo(sc_pipe, 0666);

    ConnectionRequest request;
    request.id = 'A';  // Replace with appropriate client ID
    strcpy(request.cs_pipe, cs_pipe);
    strcpy(request.sc_pipe, sc_pipe);

    if (argc > 2) {
        if (strcmp(argv[2], "-b") == 0 && argc > 3) {
            // Batch mode
            request.wsize = atoi(argv[4]);  // Convert WSIZE to integer
            if (batch_mode(open(cs_pipe, O_WRONLY), open(sc_pipe, O_RDONLY), argv[3]) == -1) {
                perror("Batch mode");
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr, "Invalid arguments for batch mode.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        // Interactive mode
        request.wsize = atoi(argv[4]);  // Convert WSIZE to integer
        mqd_t mq = mq_open(argv[1], O_RDWR, 0666, NULL);
        if (mq == -1) {
            perror("mq_open");
            exit(EXIT_FAILURE);
        }

        printf("/interactive:");
        if (interactive_mode(open(cs_pipe, O_WRONLY), open(sc_pipe, O_RDONLY), mq) == -1) {
            perror("Interactive mode");
            exit(EXIT_FAILURE);
        }

        mq_close(mq);
    }

    // Cleanup
    unlink(cs_pipe);
    unlink(sc_pipe);
    return 0;
}

int batch_mode(int cs_fd, int sc_fd, const char *comfile) {
    FILE *file = fopen(comfile, "r");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    char command[MAX_MSG_SIZE + 1];
    while (fgets(command, sizeof(command), file) != NULL) {
        // Remove newline character from the command
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

        // Send command to server
        Message command_msg;
        strncpy(command_msg.msg_size, command, sizeof(command_msg.msg_size));
        write(cs_fd, &command_msg, sizeof(Message));

        // Receive and print the result
        Message result_msg;
        ssize_t bytes_received;
        while ((bytes_received = read(sc_fd, &result_msg, sizeof(Message))) > 0) {
            printf("Result for command '%s':\n%s\n", command, result_msg.msg_size);
        }
    }

    fclose(file);
    return 0;
}

int interactive_mode(int cs_fd, int sc_fd, mqd_t mq) {
    // Implement logic for interactive mode
    char command[MAX_MSG_SIZE];
    while (1) {
        printf("Enter command (or 'quit' to exit): ");
        fgets(command, sizeof(command), stdin);

        // Check if the user wants to quit
        if (strncmp(command, "quit", 4) == 0) {
            // Send quit request to server
            Message quit_msg;
            strncpy(quit_msg.msg_size, "quit", sizeof(quit_msg.msg_size));
            write(cs_fd, &quit_msg, sizeof(Message));
            break;
        }

        // Send command to server
        Message command_msg;
        strncpy(command_msg.msg_size, command, sizeof(command_msg.msg_size));
        mq_send(mq, command_msg.msg_size, sizeof(command_msg.msg_size) + 1, 0);

        // Receive and print the result
        Message result_msg;
        ssize_t bytes_received;
        while ((bytes_received = read(sc_fd, &result_msg, sizeof(Message))) > 0) {
            printf("Result:\n%s\n", result_msg.msg_size);
        }
    }

    // Cleanup and exit
    close(cs_fd);
    return 0;
}