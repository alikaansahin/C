#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>

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

void handle_connection_request(mqd_t mq, struct Message *msg);
void handle_command_line(mqd_t mq, int cs_pipe_fd, int sc_pipe_fd, struct Message *msg);

mqd_t server_mq;

void cleanup() {
    mq_close(server_mq);
    mq_unlink("/comserver");
    exit(EXIT_SUCCESS);
}

void handle_sigint(int sig) {
    cleanup();
}

int main() {
    signal(SIGINT, handle_sigint);

    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct Message) + MAXCOMLINE;

    server_mq = mq_open("/comserver", O_RDWR | O_CREAT, 0666, &attr);

    if (server_mq == (mqd_t)-1) {
        perror("Server: mq_open");
        exit(EXIT_FAILURE);
    }

    while (1) {
        struct Message *msg = malloc(sizeof(struct Message) + MAXCOMLINE);
        ssize_t received = mq_receive(server_mq, (char *)msg, attr.mq_msgsize, NULL);

        if (received == -1) {
            perror("Server: mq_receive");
            free(msg);
            continue;
        }

        int cs_pipe[2];
        int sc_pipe[2];

        if (pipe(cs_pipe) == -1 || pipe(sc_pipe) == -1) {
            perror("pipe");
            free(msg);
            continue;
        }

        switch (msg->type) {
            case CONREQUEST:
                handle_connection_request(server_mq, msg);
                break;
            case COMLINE:
                handle_command_line(server_mq, cs_pipe[0], sc_pipe[1], msg);
                break;
                // Add cases for other message types as needed
        }

        close(cs_pipe[0]);
        close(cs_pipe[1]);
        close(sc_pipe[0]);
        close(sc_pipe[1]);

        free(msg);
    }

    cleanup();
}

void handle_connection_request(mqd_t mq, struct Message *msg) {
    // Handle connection request logic
    // You might want to fork a new process for each client and communicate with them using a separate queue
    // Example:
    pid_t child_pid = fork();

    if (child_pid == 0) {
        // Child process
        // Set up a new queue for communication with this specific client
        struct mq_attr client_attr;
        client_attr.mq_maxmsg = 10;
        client_attr.mq_msgsize = sizeof(struct Message) + MAXCOMLINE;

        // Generate a unique client queue name (you might want to use a more sophisticated method)
        char client_queue_name[MAXFILENAME];
        sprintf(client_queue_name, "/client%d", getpid());

        // Open the client queue for writing
        mqd_t client_mq = mq_open(client_queue_name, O_RDWR | O_CREAT, 0666, &client_attr);
        if (client_mq == (mqd_t)-1) {
            perror("Child: mq_open");
            exit(EXIT_FAILURE);
        }

        // Send a CONREPLY message to the client with the new queue name
        struct Message conreply_msg;
        conreply_msg.type = CONREPLY;
        conreply_msg.length = sizeof(struct Message) + strlen(client_queue_name) + 1;
        strcpy(conreply_msg.data, client_queue_name);

        mq_send(mq, (const char *)&conreply_msg, conreply_msg.length, 0);

        // Close unnecessary file descriptors (the server queue is not needed in the child)
        mq_close(mq);

        // Execute the command line interpreter or desired logic
        // Example:
        execlp("cominterpreter", "cominterpreter", client_queue_name, (char *)NULL);

        // This part will only be reached if execlp fails
        perror("execlp");
        exit(EXIT_FAILURE);
    } else if (child_pid > 0) {
        // Parent process
        // Store information about the client, e.g., queue name, in a data structure
    } else {
        perror("fork");
    }
}


// Inside the handle_command_line function

void handle_command_line(mqd_t mq, int cs_pipe_fd, int sc_pipe_fd, struct Message *msg) {
    // Extract command line from the message
    char *command_line = msg->data;

    // Set up pipes for communication with the parent process
    int pipes[2];
    if (pipe(pipes) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();

    if (child_pid == 0) {
        // Child process

        // Close read end of the pipe, since we will write to it
        close(pipes[0]);

        // Redirect standard output to the write end of the pipe
        if (dup2(pipes[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        // Close the original file descriptor
        close(pipes[1]);

        // Execute the command
        if (execlp("/bin/sh", "/bin/sh", "-c", command_line, (char *)NULL) == -1) {
            perror("execlp");
            exit(EXIT_FAILURE);
        }
    } else if (child_pid > 0) {
        // Parent process

        // Close the write end of the pipe, since we will read from it
        close(pipes[1]);

        // Read the result from the pipe
        char buffer[MAXCOMLINE];
        ssize_t bytes_read = read(pipes[0], buffer, sizeof(buffer));

        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        // Close the read end of the pipe
        close(pipes[0]);

        // Prepare the COMRESULT message with the result
        struct Message comresult_msg;
        comresult_msg.type = COMRESULT;
        comresult_msg.length = sizeof(struct Message) + bytes_read;
        memcpy(comresult_msg.data, buffer, bytes_read);

        // Send the COMRESULT message to the client
        mq_send(mq, (const char *)&comresult_msg, comresult_msg.length, 0);
    } else {
        perror("fork");
    }
}
