#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

#define SERVER_MQ "/server_mq"
#define MAX_COMLINE 1024
#define MAX_MSG_SIZE (sizeof(struct Message))
#define MAX_ARGS 10

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

// Function prototypes
void server_loop(mqd_t mq);
void handle_client(char* cs_pipe, char* sc_pipe, unsigned int wsize);
void execute_command(const char* command, int sc_fd, unsigned int wsize);
void cleanup_and_exit();

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <MQNAME>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* mq_name = argv[1];
    mqd_t mq;
    struct mq_attr attr;

    // Set up the message queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    // Create the message queue
    mq = mq_open(mq_name, O_CREAT | O_RDONLY, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("Server: mq_open failed");
        exit(EXIT_FAILURE);
    }

    // Main server loop
    server_loop(mq);

    // Cleanup
    mq_close(mq);
    mq_unlink(mq_name);
    return 0;
}

void server_loop(mqd_t mq) {
    struct Message msg;
    while (1) {
        // Wait for a message from a client
        printf("Server: Waiting for a message\n");
        if (mq_receive(mq, (char*)&msg, MAX_MSG_SIZE, NULL) == -1) {
            perror("Server: mq_receive failed");
            continue;
        }
        printf("Server: Received a message of Type\n", msg.type);
        // Handle connection request
        if (msg.type == CONREQUEST) {
            // Extract client details from the message
            char cs_pipe[MAX_COMLINE], sc_pipe[MAX_COMLINE];
            unsigned int wsize;
            sscanf(msg.data, "%s %s %u", cs_pipe, sc_pipe, &wsize);

            pid_t pid = fork();
            if (pid == 0) { // Child process
                // Handle client requests
                printf("Child process handling client\n", getpid());
                handle_client(cs_pipe, sc_pipe, wsize);
                printf("Child process exiting\n");
                exit(EXIT_SUCCESS);
            }
        } else if (msg.type == QUITALL) {
            // Handle global quit request
            cleanup_and_exit();
        }
    }
}

void handle_client(char* cs_pipe, char* sc_pipe, unsigned int wsize) {
    // Open named pipes
    int cs_fd = open(cs_pipe, O_RDONLY);
    int sc_fd = open(sc_pipe, O_WRONLY);

    // Verify pipes are open
    if (cs_fd == -1 || sc_fd == -1) {
        perror("Child: Failed to open pipes");
        exit(EXIT_FAILURE);
    }

    // Send connection reply to client
    struct Message reply;
    reply.type = CONREPLY;
    strcpy(reply.data, "Connected");
    write(sc_fd, &reply, sizeof(reply));

    // Handle client commands
    char command[MAX_COMLINE];
    while (read(cs_fd, command, MAX_COMLINE) > 0) {
        // Check for quit request
        if (strncmp(command, "quit", 4) == 0) {
            // Send quit reply and exit
            struct Message quit_reply;
            quit_reply.type = QUITREPLY;
            write(sc_fd, &quit_reply, sizeof(quit_reply));
            break;
        } else {
            // Execute command and send results
            execute_command(command, sc_fd, wsize);
        }
    }

    // Cleanup
    close(cs_fd);
    close(sc_fd);
}

void execute_command(const char* command, int sc_fd, unsigned int wsize) {
    char tempFilePath[] = "/tmp/comserver_outputXXXXXX";
    int tempFileDescriptor = mkstemp(tempFilePath);

    if (tempFileDescriptor < 0) {
        perror("Failed to create a temporary file for command output");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(tempFileDescriptor);
        unlink(tempFilePath);
        return;
    }

    if (pid == 0) { // Child process
        // Redirect stdout and stderr to the temp file
        dup2(tempFileDescriptor, STDOUT_FILENO);
        dup2(tempFileDescriptor, STDERR_FILENO);
        close(tempFileDescriptor);

        // Execute the command
        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }

    // Parent process waits for the command to finish
    waitpid(pid, NULL, 0);
    lseek(tempFileDescriptor, 0, SEEK_SET); // Reset file descriptor position for reading

    char buffer[wsize];
    ssize_t bytesRead;
    while ((bytesRead = read(tempFileDescriptor, buffer, wsize)) > 0) {
        if (write(sc_fd, buffer, bytesRead) < 0) {
            perror("Error writing to client");
            break;
        }
    }

    close(tempFileDescriptor);
    unlink(tempFilePath); // Clean up the temporary file
}

void cleanup_and_exit() {
    if (SERVER_MQ != (mqd_t)-1) {
        mq_close(SERVER_MQ);
        mq_unlink(SERVER_MQ);
    }
    printf("Server cleanup completed. Exiting.\n");
    exit(EXIT_SUCCESS);
}


