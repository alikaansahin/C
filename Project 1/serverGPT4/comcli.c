#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>

#define MAX_COMLINE 1024
#define DEFAULT_WSIZE 512

struct Message {
    unsigned int length;
    unsigned char type;
    unsigned char padding[3];
    char data[MAX_COMLINE];
};

enum MessageType {
    CONREQUEST = 1,
    CONREPLY,
    COMLINE,
    COMRESULT,
    QUITREQ,
    QUITREPLY,
    QUITALL
};

void send_message(mqd_t mq, enum MessageType type, const char* data, unsigned int wsize);
void receive_and_print_result(int sc_fd);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <MQNAME> [-b COMFILE] [-s WSIZE]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* mq_name = argv[1];
    char* comfile = NULL;
    unsigned int wsize = DEFAULT_WSIZE;

    // Parse command line arguments for batch mode and write size
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            comfile = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            wsize = (unsigned int)atoi(argv[++i]);
        }
    }

    mqd_t mq = mq_open(mq_name, O_WRONLY);
    if (mq == (mqd_t)-1) {
        perror("Client: mq_open");
        exit(EXIT_FAILURE);
    }
    
    char cs_pipe_name[256], sc_pipe_name[256];
    sprintf(cs_pipe_name, "/tmp/cs_pipe_%d", getpid());
    sprintf(sc_pipe_name, "/tmp/sc_pipe_%d", getpid());

    // Create named pipes
    mkfifo(cs_pipe_name, 0666);
    mkfifo(sc_pipe_name, 0666);

    // Send connection request to server
    char connectMsg[MAX_COMLINE];
    sprintf(connectMsg, "%s %s %u", cs_pipe_name, sc_pipe_name, wsize);
    send_message(mq, CONREQUEST, connectMsg, wsize);

    // Open the SC pipe to receive messages from the server
    int sc_fd = open(sc_pipe_name, O_RDONLY);
    if (sc_fd == -1) {
        perror("Client: open sc_pipe");
        exit(EXIT_FAILURE);
    }

    // Check for connection reply
    receive_and_print_result(sc_fd);

    FILE* fp = stdin;
    if (comfile) {
        fp = fopen(comfile, "r");
        if (!fp) {
            perror("Failed to open command file");
            exit(EXIT_FAILURE);
        }
    }

    // Main loop to read and send commands
    char command[MAX_COMLINE];
    int cs_fd = open(cs_pipe_name, O_WRONLY);
    while (fgets(command, MAX_COMLINE, fp)) {
        // Remove newline character
        command[strcspn(command, "\n")] = '\0';
        write(cs_fd, command, strlen(command) + 1);
        receive_and_print_result(sc_fd);
    }

    // Cleanup
    close(cs_fd);
    close(sc_fd);
    unlink(cs_pipe_name);
    unlink(sc_pipe_name);
    mq_close(mq);

    return 0;
}

void send_message(mqd_t mq, enum MessageType type, const char* data, unsigned int wsize) {
    struct Message msg;
    msg.type = type;
    strncpy(msg.data, data, MAX_COMLINE - 1);
    msg.data[MAX_COMLINE - 1] = '\0';
    mq_send(mq, (const char*)&msg, sizeof(msg), 0);
}

void receive_and_print_result(int sc_fd) {
    char buffer[MAX_COMLINE];
    memset(buffer, 0, MAX_COMLINE);
    read(sc_fd, buffer, MAX_COMLINE - 1);
    printf("%s\n", buffer);
}