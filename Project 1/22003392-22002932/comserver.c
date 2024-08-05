#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/wait.h>

enum MessageType {
    CONREQUEST  = 1,
    CONREPY     = 2,
    COMLINE     = 3,
    COMRESULT   = 4,
    QUITREQ     = 5,
    QUITREPLY   = 6,
    QUITALL     = 7
};

typedef struct {
   int  length;
   char type;
   char padding[3];
   char data[1024];
} Message;

typedef struct {
   char scPipe[256];
   char csPipe[256];
   int  clientID;
   int  wsize;
} ConnectionRequest;

void printMessage(Message *msg) {
   printf("Message length: %d\n", msg->length);
   printf("Message type: %c\n", msg->type);
   printf("Message data: %s\n", msg->data);
}
void decipherConnectionRequest(Message *msg, int *client_pid, int *wsize, char csPipe[], char scPipe[]) {
   if (msg->type != 1) {
       fprintf(stderr, "Invalid message type for connection request: %c\n", msg->type);
       return;
   }

   sscanf(msg->data, "%d%d%s%s", client_pid, wsize, csPipe, scPipe);

   printf("pid: %d\n", &client_pid);
   printf("wsize: %d\n", &wsize);
   printf("csPipe:%s\n", csPipe);
   printf("scPipe:%s\n", scPipe);

}

void handle_conreply(Message *msg) {
   // Handle CONREPLY message
}


void handle_comline(Message *msg) {
   // Handle COMLINE message
}

void handleMessage(Message *msg) {
   switch (msg->type) {
        case 1:  // CONREQUEST
           int client_pid;
           int wsize;
           char csPipe[256];
           char scPipe[256];
           decipherConnectionRequest(msg, client_pid, wsize, csPipe, scPipe);
           break;
        case 2:  // CONREPLY
           handle_conreply(msg);
           break;
        /* case 3:  // COMLINE
           handle_comline(msg);
           break;
        case 4:  // COMRESULT
           handle_comresult(msg);
           break;
         case 5:  // QUITREQ
            handle_quitreq(msg);
            break;
        case 6:  // QUITREPLY
            handle_quitreply(msg);
            break;
        case 7:  // QUITALL
            handle_quitall(msg);
            break; */
       default:
           fprintf(stderr, "Unknown message type: %c\n", msg->type);
           break;
   }
}

int handleClient(ConnectionRequest *req) {

   printf("This is the handleClient function and this is child with pid: %d\n", getpid());
   printf("scPipe: %s\n", req->scPipe);
   printf("csPipe: %s\n", req->csPipe);
   printf("clientID: %d\n", req->clientID);
   printf("wsize: %d\n", req->wsize);

   if (req->csPipe[0] == ' ') {
       memmove(req->csPipe, req->csPipe + 1, strlen(req->csPipe));
   }
   if (req->scPipe[0] == ' ') {
       memmove(req->scPipe, req->scPipe + 1, strlen(req->scPipe));
   }

   if (req->csPipe[strlen(req->csPipe) - 1] == ' ' || req->csPipe[strlen(req->csPipe) - 1] == '\n') {
       req->csPipe[strlen(req->csPipe) - 1] = '\0';
   }
   if (req->scPipe[strlen(req->scPipe) - 1] == ' ' || req->scPipe[strlen(req->scPipe) - 1] == '\n') {
       req->scPipe[strlen(req->scPipe) - 1] = '\0';
   }
  
   char cs[256];
   char sc[256];
   
   strcpy(cs, req->csPipe);
   strcpy(sc, req->scPipe);

   printf("Opening server-to-client pipe for writing\n");
   printf("the pipe name is: %s%s\n", req->scPipe, req->scPipe);

   int sc_fd = open(sc, O_WRONLY);
   if (sc_fd == -1) {
       perror("open");
       exit(1);
   } 


   printf("Opened server-to-client pipe for writing\n");

   char response[256];
   sprintf(response, "Welcome client %d this is child %d", req->clientID, getpid());
   if (write(sc_fd, response, strlen(response) + 1) == -1) {
       perror("write");
       exit(1);
   }
   printf("Sent response to client: %s\n", response);

   char *temp_filename = tmpnam(NULL);
   printf("Temporary file: %s\n", temp_filename);


   int temp_file = open(temp_filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
   if (temp_file == -1) {
       perror("open");
       exit(1);
   }
  
   int cs_fd = open(req->csPipe, O_RDONLY);
   if (cs_fd == -1) {
       perror("open");
       exit(1);
   }


   while (1) {
       printf("Waiting for a command from the client\n");
       char command[256];
       if (read(cs_fd, command, sizeof(command)) == -1) {
           perror("read");
           exit(1);
       }

       printf("Received command from client: %s\n", command);

       if (strcmp(command, "quit") == 0) {
           sprintf(response, "quit-back");
           if (write(sc_fd, response, strlen(response) + 1) == -1) {
               perror("write");
               return 1;
           }
           close(sc_fd);
           return 1;
       }


   printf("Checking if the command has a pipe\n");

   int command_has_pipe = 0;
   char *command1;
   char *command2;

   if (strchr(command, '|') != NULL) {
       command_has_pipe = 1;

       char command_copy[256];
       strcpy(command_copy, command);

       char *token = strtok(command_copy, "|");
       command1 = token;

       printf("Step 1: %s\n", command1);

       token = strtok(NULL, "|");
       command2 = (token != NULL) ? token : "";

       printf("Step 2: %s\n", command2);


       while (*command1 && (*command1 == ' ' || *command1 == '\n')) {
           command1++;
       }

       printf("Step 3: %s\n", command1);


       char *end1 = command1 + strlen(command1) - 1;
       while (end1 > command1 && (*end1 == ' ' || *end1 == '\n')) {
           end1--;
       }
       *(end1 + 1) = '\0';

       printf("Step 4: %s\n", command1);

       while (*command2 && (*command2 == ' ' || *command2 == '\n')) {
           command2++;
       }

       printf("Step 5: %s\n", command2);


       char *end2 = command2 + strlen(command2) - 1;
       printf("End2: %s\n", end2);
       while (end2 > command2 && (*end2 == ' ' || *end2 == '\n')) {
           printf("Step 6:");
           end2--;
       }
       *(end2 + 1) = '\0';


       printf("Command1: %s\n", command1);
       printf("Command2: %s\n", command2);
       printf("command is: %s\n", command);
   }

       if (command_has_pipe == 0) {   
 
           pid_t pid = fork();
           if (pid == -1) {
               perror("fork");
               exit(1);
           } else if (pid == 0) {
  
               if (dup2(temp_file, STDOUT_FILENO) == -1) {
                   perror("dup2");
                   exit(1);
               }

               char *args[] = {"/bin/sh", "-c", command, NULL};
               execv(args[0], args);
               perror("execv");
               exit(1);
           }
           else {
               int status;
               waitpid(pid, &status, 0);
               printf ("Child process exited with status %d\n", status);

               printf("Opening the temporary file\n");
               FILE *temp_file = fopen(temp_filename, "r");
               if (temp_file == NULL) {
                   perror("fopen");
                   exit(1);
               }
               printf("Opened the temporary file\n");

               char buffer[req->wsize];

               int sc_fd = open(req->scPipe, O_WRONLY);
               if (sc_fd == -1) {
                   perror("open");
                   exit(1);
               } 
               ssize_t bytesRead;
               while ((bytesRead = fread(buffer, 1, req->wsize, temp_file)) > 0) {
                   printf("Read %ld bytes from the temporary file\n", bytesRead);

                   printf("Buffer: %s\n", buffer);

                   printf("Size of buffer: %ld\n", req->wsize);
                   write(sc_fd, buffer, bytesRead);
                   printf("Wrote %ld bytes to the client\n", bytesRead); 
                   if (bytesRead < req->wsize) {
                       break;
                   }
               }

               printf("Sent response to client\n");

               if (ferror(temp_file)) {
                   perror("fread");
                   exit(1);
               }

               printf(" after the error check\n");

               fclose(temp_file);
               printf("Closed the temporary file\n");
           }
       }
      
       else {
           pid_t pid = fork();
           if (pid == -1) {
               perror("fork");
               exit(1);
           } 
           else if (pid == 0) {

               if (dup2(temp_file, STDOUT_FILENO) == -1) {
                   perror("dup2");
                   exit(1);
               }

               char *args[] = {"/bin/sh", "-c", command1, NULL};
               execv(args[0], args);
               perror("execv");
               exit(1);
           }
           else {
               int status;
               waitpid(pid, &status, 0);
               printf ("Child process exited with status %d\n", status);

               printf("Opening the temporary file\n");
               FILE *temp_file = fopen(temp_filename, "r");
               if (temp_file == NULL) {
                   perror("fopen");
                   exit(1);
               }
               printf("Opened the temporary file\n");

               char buffer[req->wsize];

               int sc_fd = open(req->scPipe, O_WRONLY);
               if (sc_fd == -1) {
                   perror("open");
                   exit(1);
               } 
               ssize_t bytesRead;
               while ((bytesRead = fread(buffer, 1, req->wsize, temp_file)) > 0) {
                   printf("Read %ld bytes from the temporary file\n", bytesRead);

                   printf("Buffer: %s\n", buffer);

                   printf("Size of buffer: %ld\n", req->wsize);
                   write(sc_fd, buffer, bytesRead);
                   printf("Wrote %ld bytes to the client\n", bytesRead); 
                   if (bytesRead < req->wsize) {
                       break;
                   }
               }

               printf("Sent response to client\n");

               if (ferror(temp_file)) {
                   perror("fread");
                   exit(1);
               }
               printf(" after the error check\n");
           }
          
           pid_t pid2 = fork();
           if (pid2 == -1) {
               perror("fork");
               exit(1);
           } 
           else if (pid2 == 0) {
               char cmd[256];
               sprintf(cmd, "%s %s", command2, temp_filename);
               printf("Command 2: %s\n", cmd);

               char *args[] = {"/bin/sh", "-c", cmd, NULL};
               execv(args[0], args);
               perror("execv");
               exit(1);
           }
           else {
               int status;
               waitpid(pid2, &status, 0);
               printf ("Child process exited with status %d\n", status);

           }
       }
   }

   return 0;
}


int main(int argc, char *argv[]) {
   if (argc != 2) {
       printf("Usage: %s MQNAME\n", argv[0]);
       return 1;
   }
   char *mqName = argv[1];
   char mqname_with_slash[256];
   if (mqName[0] != '/') {
       sprintf(mqname_with_slash, "/%s", mqName);
   } else {
       strncpy(mqname_with_slash, mqName, sizeof(mqname_with_slash) - 1);
   }
   printf("MQNAME: %s\n", mqName);
   printf("MQNAME_WITH_SLASH: %s\n", mqname_with_slash);


   mqd_t messageQueue = mq_open(mqname_with_slash, O_CREAT | O_RDONLY, 0666, NULL);
   if (messageQueue == (mqd_t)-1) {
       perror("mq_open");
       return 1;
   }

   struct mq_attr attr;
   if (mq_getattr(messageQueue, &attr) == -1) {
       perror("mq_getattr");
       return 1;
   }

   char *buffer = malloc(attr.mq_msgsize);
   if (buffer == NULL) {
       perror("malloc");
       return 1;
   }

   while (1) {
       ssize_t bytesRead = mq_receive(messageQueue, buffer, attr.mq_msgsize , NULL);
       if (bytesRead == -1) {
           perror("mq_receive");
           return 1;
       }

       ConnectionRequest *req = (ConnectionRequest *)buffer;

       printf("Message data: %s\n", req->scPipe);
       printf("Message data: %s\n", req->csPipe);
       printf("Message data: %d\n", req->clientID);
       printf("Message data: %d\n", req->wsize);

       int client_pid = req->clientID;
       printf("Received connection request from client %d\n", client_pid);

       pid_t pid = fork();
       if (pid == -1) {
           perror("fork");
           free(buffer);
           return 1;
       } else if (pid == 0) {
           printf("Child process created to handle client %d\n", client_pid);

           printf("scPipe: %s\n", req->scPipe);
           printf("csPipe: %s\n", req->csPipe);
           handleClient(req);
           exit(0);
       }
   }
   return 0;
}
