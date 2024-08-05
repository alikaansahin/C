#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <getopt.h>

#define DEFAULT_WSIZE 1024  // Change it to measure time
#define MAX_MSG_SIZE 8192

typedef struct {
   int length;
   char type;
   char padding[3];
   char data[1024];
} Message;

typedef struct {
   char scPipe[256];
   char csPipe[256];
   int clientID;
   int WSIZE;
} ConnectionRequest;

void decipherConnectionRequest(Message *msg) {

   if (msg->type != 'C') {
       fprintf(stderr, "Invalid message type for connection request: %c\n", msg->type);
       return;
   }

   int pid, WSIZE;
   char csPipe[256], scPipe[256];

   sscanf(msg->data, "%d%d%s%s", &pid, &WSIZE, csPipe, scPipe);

   printf("pid: %d\n", pid);
   printf("WSIZE: %d\n", WSIZE);
   printf("csPipe: %s\n", csPipe);
   printf("scPipe: %s\n", scPipe);
}

Message createConnectionRequest(ConnectionRequest *req, char *scPipe, char *csPipe, int WSIZE) {
   int length = sizeof(ConnectionRequest);
   char type = 'C';
   char padding[3] = {0, 0, 0};
   char data[1024];

   sprintf(data, "%d %d %s %s", getpid(), WSIZE, scPipe, csPipe);

   Message msg = {length, type, padding, ""};
   strncpy(msg.data, data, 1024);

   printf("pid: %d\n", getpid());
   printf("WSIZE: %d\n", WSIZE);
   printf("csPipe: %s\n", csPipe);
   printf("scPipe: %s\n", scPipe);

   return msg;
}

int main(int argc, char *argv[]) {
   if (argc < 2) {
       printf("Usage: %s MQNAME [-b COMFILE] [-s WSIZE]\n", argv[0]);
       return 1;
   }

   char *mqname = argv[1];
   char *comfile = NULL;
   int WSIZE = DEFAULT_WSIZE;

   int opt;
   while ((opt = getopt(argc, argv, "b:s:")) != -1) {
       switch (opt) {
           case 'b':   
               break;
           case 's':
               WSIZE = atoi(optarg);
               break;
           default:
               printf("Usage: %s MQNAME [-b COMFILE] [-s WSIZE]\n", argv[0]);
               return 1;
       }
   }

   printf("opening the message queue and the pipes \n");

   char scPipe[256], csPipe[256];
   sprintf(scPipe, "/tmp/sc_pipe_%d", getpid());
   sprintf(csPipe, "/tmp/cs_pipe_%d", getpid());

   if (mkfifo(scPipe, 0666) == -1) {
       perror("mkfifo");
       exit(1);
   }

   printf("Opened the server-to-client pipe\n");
   printf("The pipe name is: %s\n", scPipe);

   if (mkfifo(csPipe, 0666) == -1) {
       perror("mkfifo");
       return 1;
   }

   mqd_t messageQueue = mq_open(mqname, O_WRONLY);
   if (messageQueue == (mqd_t)-1) {
       perror("mq_open");
       return 1;
   }

   printf("MQNAME: %s\n", mqname);

   ConnectionRequest req = { .clientID = getpid(), .WSIZE = 1024 };


   Message msg = createConnectionRequest(&req, scPipe, csPipe, WSIZE);

   printf("Message data: %s\n", msg.data);
   decipherConnectionRequest(&msg);

   strncpy(req.scPipe, scPipe, sizeof(req.scPipe) - 1);
   strncpy(req.csPipe, csPipe, sizeof(req.csPipe) - 1);

   if (mq_send(messageQueue, (char *)&req, sizeof(req), 0) == -1) {
       perror("mq_send");
       return 1;
   }

   printf("Waiting for a response from the server...\n");
   int sc_fd = open(scPipe, O_RDONLY);
   if (sc_fd == -1) {
       perror("open");
       return 1;
   }
   printf("got a response from the server\n");


   char response[MAX_MSG_SIZE];
   if (read(sc_fd, response, sizeof(response)) == -1) {
       perror("read");
       return 1;
   }

   printf("Received response from server: %s\n", response);

   int cs_fd = open(csPipe, O_WRONLY);
   if (cs_fd == -1) {
       perror("open");
       return 1;
   }

   while (1) {
       char request[MAX_MSG_SIZE];
       printf("Enter a command: ");
       fgets(request, sizeof(request), stdin);
       request[strlen(request) - 1] = '\0';


       if (write(cs_fd, request, strlen(request) + 1) == -1) {
           perror("write");
           return 1;
       }

       if (strcmp(request, "quit") == 0) {          
           break;
       }

       char buffer[WSIZE];
       ssize_t bytes_read;
       while ((bytes_read = read(sc_fd, buffer, sizeof(buffer))) > 0) {
           if (write(STDOUT_FILENO, buffer, bytes_read) != bytes_read) {
               perror("write");
               exit(1);
           }
           if (bytes_read < 0) {
               perror("read");
               exit(1);
           }
           if (bytes_read < WSIZE) {
               break;
           }

           printf("bytes_read: %ld\n", bytes_read);
           printf("received message from the server");
       }
       if (bytes_read == -1) {
           perror("read");
           exit(1);
       }  
   }
   return 0;
}
