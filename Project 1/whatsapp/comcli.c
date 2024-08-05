#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <getopt.h>


#define DEFAULT_WSIZE 1024
#define MAX_MSG_SIZE 8192


typedef struct {
   char sc_pipe[256];
   char cs_pipe[256];
   int client_id;
   int wsize;
} ConnectionRequest;


typedef struct {
   int length;
   char type;
   char padding[3]; // 3 bytes of padding
   char data[1024];
} Message;




Message create_connection_request(ConnectionRequest *req, char *sc_pipe, char *cs_pipe, int wsize) {
   // Create a connection request using the Message struct
   int length = sizeof(ConnectionRequest);
   char type = 'C';
   char padding[3] = {0, 0, 0};
   char data[1024];  // Adjusted size
   sprintf(data, "%d %d %s %s", getpid(), wsize, sc_pipe, cs_pipe);
   Message msg = {length, type, padding, ""};
   strncpy(msg.data, data, 1024);  // Adjusted size
   printf("pid: %d\n", getpid());
   printf("wsize: %d\n", wsize);
   printf("cs_pipe: %s\n", cs_pipe);
   printf("sc_pipe: %s\n", sc_pipe);
   return msg;
}


void decipher_connection_request(Message *msg) {
   // Check if the message type is 'C' for CONREQUEST
   if (msg->type != 'C') {
       fprintf(stderr, "Invalid message type for connection request: %c\n", msg->type);
       return;
   }


   // Extract the pid, wsize, cs_pipe, and sc_pipe from the data segment
   int pid, wsize;
   char cs_pipe[256], sc_pipe[256];


   sscanf(msg->data, "%d%d%s%s", &pid, &wsize, cs_pipe, sc_pipe);


   printf("pid: %d\n", pid);
   printf("wsize: %d\n", wsize);
   printf("cs_pipe: %s\n", cs_pipe);
   printf("sc_pipe: %s\n", sc_pipe);
}


int main(int argc, char *argv[]) {
   if (argc < 2) {
       printf("Usage: %s MQNAME [-b COMFILE] [-s WSIZE]\n", argv[0]);
       return 1;
   }


   char *mqname = argv[1];
   char *comfile = NULL;
   int wsize = DEFAULT_WSIZE;


   int opt;
   while ((opt = getopt(argc, argv, "b:s:")) != -1) {
       switch (opt) {
           case 'b':   
               break;
           case 's':
               wsize = atoi(optarg);
               break;
           default:
               printf("Usage: %s MQNAME [-b COMFILE] [-s WSIZE]\n", argv[0]);
               return 1;
       }
   }


   printf("opening the message queue and the pipes \n");


   // Create two named pipes
   char sc_pipe[256], cs_pipe[256];
   sprintf(sc_pipe, "/tmp/sc_pipe_%d", getpid());
   sprintf(cs_pipe, "/tmp/cs_pipe_%d", getpid());


   // Create the server-to-client pipe
   if (mkfifo(sc_pipe, 0666) == -1) {
       perror("mkfifo");
       exit(1);
   }


   printf("Opened the server-to-client pipe\n");
   printf("the pipe name is: %s%s\n", sc_pipe, sc_pipe);


   // Create the client-to-server pipe
   if (mkfifo(cs_pipe, 0666) == -1) {
       perror("mkfifo");
       return 1;
   }


   // Open the message queue
   mqd_t mq = mq_open(mqname, O_WRONLY);
   if (mq == (mqd_t)-1) {
       perror("mq_open");
       return 1;
   }


   // print message queue name opened successfully
   printf("MQNAME: %s\n", mqname);


   // Send a connection request to the server
   // Create a connection request
   ConnectionRequest req = { .client_id = getpid(), .wsize = 1024 };


   // use the Message struct to send the connection request to the server
   Message msg = create_connection_request(&req, sc_pipe, cs_pipe, wsize);
   //print the data of the message
   printf("Message data: %s\n", msg.data);
   decipher_connection_request(&msg);


   strncpy(req.sc_pipe, sc_pipe, sizeof(req.sc_pipe) - 1);
   strncpy(req.cs_pipe, cs_pipe, sizeof(req.cs_pipe) - 1);


/*     // Send the connection request to the server
   if (mq_send(mq, (char *)&msg, sizeof(msg), 0) == -1) {
       perror("mq_send");
       return 1;
   } */


   // Send the connection request to the server using the message queue with the ConnectionRequest struct
   if (mq_send(mq, (char *)&req, sizeof(req), 0) == -1) {
       perror("mq_send");
       return 1;
   }




   // Wait for a response from the server
   printf("Waiting for a response from the server...\n");
   int sc_fd = open(sc_pipe, O_RDONLY);
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


   // Send a request to the server to execute the command entered by the user
   int cs_fd = open(cs_pipe, O_WRONLY);
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
       // if the user enters "exit", break the loop
       if (strcmp(request, "quit") == 0) {          
           break;
       }


       // Read the response from the server
       // Read the pipe in chunks and print each chunk to the standard output
       char buffer[wsize];
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
           if (bytes_read < wsize) {
               break;
           }


           printf("bytes_read: %ld\n", bytes_read);
           printf("received message from the server");
       }
       if (bytes_read == -1) {
           perror("read");
           exit(1);
       }


       // Close the pipe      




   }
   return 0;
}
