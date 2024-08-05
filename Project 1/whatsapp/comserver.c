#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/wait.h>


typedef struct {
   char sc_pipe[256];
   char cs_pipe[256];
   int client_id;
   int wsize;
} ConnectionRequest;


typedef struct {
   int length;
   char type;
   char padding[3];
   char data[1024];
} Message;


enum MessageType {
   CONREQUEST = 'C',
   CONREPLY = 'R',
   COMLINE = 'L'
};


void print_message(Message *msg) {
   printf("Message length: %d\n", msg->length);
   printf("Message type: %c\n", msg->type);
   printf("Message data: %s\n", msg->data);
}
void decipher_connection_request(Message *msg, int *client_pid, int *wsize, char cs_pipe[], char sc_pipe[]) {
   // Check if the message type is 'C' for CONREQUEST
   if (msg->type != 'C') {
       fprintf(stderr, "Invalid message type for connection request: %c\n", msg->type);
       return;
   }


   // Extract the pid, wsize, cs_pipe, and sc_pipe from the data segment


   sscanf(msg->data, "%d%d%s%s", client_pid, wsize, cs_pipe, sc_pipe);


   printf("pid: %d\n", &client_pid);
   printf("wsize: %d\n", &wsize);
   printf("cs_pipe:%s\n", cs_pipe);
   printf("sc_pipe:%s\n", sc_pipe);

}

void handle_conreply(Message *msg) {
   // Handle CONREPLY message
}


void handle_comline(Message *msg) {
   // Handle COMLINE message
}


void handle_message(Message *msg) {
   switch (msg->type) {
       case 'C':  // CONREQUEST
           int client_pid;
           int wsize;
           char cs_pipe[256];
           char sc_pipe[256];
           decipher_connection_request(msg, client_pid, wsize, cs_pipe, sc_pipe);
           break;
       case 'R':  // CONREPLY
           handle_conreply(msg);
           break;
       case 'L':  // COMLINE
           handle_comline(msg);
           break;
       default:
           fprintf(stderr, "Unknown message type: %c\n", msg->type);
           break;
   }
}




/* int handle_client(ConnectionRequest *req) {
   int sc_fd = open(req->sc_pipe, O_WRONLY);
   if (sc_fd == -1) {
       perror("open");
       exit(1);
   }


   char response[256];
   sprintf(response, "Welcome client %d this is child %d", req->client_id, getpid());
   if (write(sc_fd, response, strlen(response) + 1) == -1) {
       perror("write");
       exit(1);
   }
   printf("Sent response to client: %s\n", response);


   int cs_fd = open(req->cs_pipe, O_RDONLY);
   if (cs_fd == -1) {
       perror("open");
       exit(1);
   }


   while (1) {
       char command[256];
       if (read(cs_fd, command, sizeof(command)) == -1) {
           perror("read");
           exit(1);
       }
       printf("Received command from client: %s\n", command);


       if (strcmp(command, "quit") == 0) {
           close(sc_fd);
           return 1;
       }


       system(command);


       sprintf(response, "%s executed", command);
       if (write(sc_fd, response, strlen(response) + 1) == -1) {
           perror("write");
           return 1;
       }
   }
   return 0;
} */


int handle_client(ConnectionRequest *req) {


   //print the data of the message received
   printf("This is the handle_client function and this is child with pid: %d\n", getpid());
   printf("sc_pipe: %s\n", req->sc_pipe);
   printf("cs_pipe: %s\n", req->cs_pipe);
   printf("client_id: %d\n", req->client_id);
   printf("wsize: %d\n", req->wsize);


   //check if the pipe names start with a space and remove it


   if (req->cs_pipe[0] == ' ') {
       memmove(req->cs_pipe, req->cs_pipe + 1, strlen(req->cs_pipe));
   }
   if (req->sc_pipe[0] == ' ') {
       memmove(req->sc_pipe, req->sc_pipe + 1, strlen(req->sc_pipe));
   }

   //check if the pipe names end with a space or a newline and remove the space or newline
   if (req->cs_pipe[strlen(req->cs_pipe) - 1] == ' ' || req->cs_pipe[strlen(req->cs_pipe) - 1] == '\n') {
       req->cs_pipe[strlen(req->cs_pipe) - 1] = '\0';
   }
   if (req->sc_pipe[strlen(req->sc_pipe) - 1] == ' ' || req->sc_pipe[strlen(req->sc_pipe) - 1] == '\n') {
       req->sc_pipe[strlen(req->sc_pipe) - 1] = '\0';
   }
  
   //create new char arrays with size 256 to ensure that the pipe names are not too long
   char cs[256];
   char sc[256];
   //transfer the pipe names from req to the new char arrays
   strcpy(cs, req->cs_pipe);
   strcpy(sc, req->sc_pipe);


   // Open the server-to-client pipe for writing
   printf("Opening server-to-client pipe for writing\n");
   printf("the pipe name is: %s%s\n", req->sc_pipe, req->sc_pipe);


   // open the pipes using the new arrays
   int sc_fd = open(sc, O_WRONLY);
   if (sc_fd == -1) {
       perror("open");
       exit(1);
   } 


   printf("Opened server-to-client pipe for writing\n");


   // Send a welcome message to the client
   char response[256];
   sprintf(response, "Welcome client %d this is child %d", req->client_id, getpid());
   if (write(sc_fd, response, strlen(response) + 1) == -1) {
       perror("write");
       exit(1);
   }
   printf("Sent response to client: %s\n", response);


   // Create a temporary file
   char *temp_filename = tmpnam(NULL);
   printf("Temporary file: %s\n", temp_filename);


   int temp_file = open(temp_filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
   if (temp_file == -1) {
       perror("open");
       exit(1);
   }


   //open another temporary file to store the output of the second command with name temp_filename2
  


   int cs_fd = open(req->cs_pipe, O_RDONLY);
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


   //print checking if the command has "|"
   printf("Checking if the command has a pipe\n");


   int command_has_pipe = 0;
   char *command1;
   char *command2;




   if (strchr(command, '|') != NULL) {
       command_has_pipe = 1;
       //copy command to a new char array
       char command_copy[256];
       strcpy(command_copy, command);


  
       // Check if the command has "|"
       char *token = strtok(command_copy, "|");
       command1 = token;


       //print step1
       printf("Step 1: %s\n", command1);


       // If there is a second part after "|"
       token = strtok(NULL, "|");
       command2 = (token != NULL) ? token : ""; // Set to empty string if NULL


       //print step2
       printf("Step 2: %s\n", command2);


       // Trim leading and trailing whitespaces for command1
       while (*command1 && (*command1 == ' ' || *command1 == '\n')) {
           command1++;
       }


       //print step3
       printf("Step 3: %s\n", command1);


       char *end1 = command1 + strlen(command1) - 1;
       while (end1 > command1 && (*end1 == ' ' || *end1 == '\n')) {
           end1--;
       }
       *(end1 + 1) = '\0';


       //print step4
       printf("Step 4: %s\n", command1);


       // Trim leading and trailing whitespaces for command2
       while (*command2 && (*command2 == ' ' || *command2 == '\n')) {
           command2++;
       }


       //print step5
       printf("Step 5: %s\n", command2);


       char *end2 = command2 + strlen(command2) - 1;
       printf("End2: %s\n", end2);
       while (end2 > command2 && (*end2 == ' ' || *end2 == '\n')) {
           // print step6
           printf("Step 6:");
           end2--;
       }
       *(end2 + 1) = '\0';


       // Print the new commands
       printf("Command 1: %s\n", command1);
       printf("Command 2: %s\n", command2);
       printf("command is: %s\n", command);
   }


       if (command_has_pipe == 0) {   
           // Create a child process to execute the command
           pid_t pid = fork();
           if (pid == -1) {
               perror("fork");
               exit(1);
           } else if (pid == 0) {
               // this is the child process


               // Redirect the standard output to the temporary file
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


               // Open the temporary file
               printf("Opening the temporary file\n");
               FILE *temp_file = fopen(temp_filename, "r");
               if (temp_file == NULL) {
                   perror("fopen");
                   exit(1);
               }
               printf("Opened the temporary file\n");


               // Create a buffer to hold the file contents
               char buffer[req->wsize];


               // Read the file contents in chunks of size wsize and write each chunk to the sc_pipe
               int sc_fd = open(req->sc_pipe, O_WRONLY);
               if (sc_fd == -1) {
                   perror("open");
                   exit(1);
               } 
               ssize_t bytes_read;
               while ((bytes_read = fread(buffer, 1, req->wsize, temp_file)) > 0) {
                   printf("Read %ld bytes from the temporary file\n", bytes_read);
                   //print the buffer
                   printf("Buffer: %s\n", buffer);
                   //print the size of the buffer
                   printf("Size of buffer: %ld\n", req->wsize);
                   write(sc_fd, buffer, bytes_read);
                   printf("Wrote %ld bytes to the client\n", bytes_read); 
                   if (bytes_read < req->wsize) {
                       break;
                   }
               }


               printf("Sent response to client\n");


               // Check for errors in fread
               if (ferror(temp_file)) {
                   perror("fread");
                   exit(1);
               }


               printf(" after the error check\n");


               // Close the temporary file
               fclose(temp_file);
               printf("Closed the temporary file\n");
           }
       }
      
       else {
           // this means that the command has a pipe and is now separated into two commands, command1 and command2
           // execute the first command and store the output in a temporary file, use a child process to execute the command
           // after the execution of the first command, execute the second command and store the output in a temporary file
           // read the contents of the temporary file and send it to the client
           // ensure that both commands are executed by different child processes and the input of the second command is the output of the first command
           // Create a child process to execute the command
           pid_t pid = fork();
           if (pid == -1) {
               perror("fork");
               exit(1);
           } else if (pid == 0) {
               // this is the child process


               // Redirect the standard output to the temporary file
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


               // Open the temporary file
               printf("Opening the temporary file\n");
               FILE *temp_file = fopen(temp_filename, "r");
               if (temp_file == NULL) {
                   perror("fopen");
                   exit(1);
               }
               printf("Opened the temporary file\n");


               // Create a buffer to hold the file contents
               char buffer[req->wsize];


               // Read the file contents in chunks of size wsize and write each chunk to the sc_pipe
               int sc_fd = open(req->sc_pipe, O_WRONLY);
               if (sc_fd == -1) {
                   perror("open");
                   exit(1);
               } 
               ssize_t bytes_read;
               while ((bytes_read = fread(buffer, 1, req->wsize, temp_file)) > 0) {
                   printf("Read %ld bytes from the temporary file\n", bytes_read);
                   //print the buffer
                   printf("Buffer: %s\n", buffer);
                   //print the size of the buffer
                   printf("Size of buffer: %ld\n", req->wsize);
                   write(sc_fd, buffer, bytes_read);
                   printf("Wrote %ld bytes to the client\n", bytes_read); 
                   if (bytes_read < req->wsize) {
                       break;
                   }
               }


               printf("Sent response to client\n");


               // Check for errors in fread
               if (ferror(temp_file)) {
                   perror("fread");
                   exit(1);
               }


               printf(" after the error check\n");


           }
           // when the code reaches here, the first command has been executed and the output has been sent to the client
           // execute the second command and send the output to the client
           // Create a child process to execute the command
           // delete the temporary file and create a new one
          
           pid_t pid2 = fork();
           if (pid2 == -1) {
               perror("fork");
               exit(1);
           } else if (pid2 == 0) {
               // this is the child process


               // Redirect the standard output to the temporary file
/*                 if (dup2(temp_file, STDOUT_FILENO) == -1) {
                   perror("dup2");
                   exit(1);
               } */


               // update command2 by adding the temp_file to the end of the command, name cmd
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


              /*  // Open the temporary file
               printf("Opening the temporary file\n");
               FILE *temp_file = fopen(temp_filename, "r");
               if (temp_file == NULL) {
                   perror("fopen");
                   exit(1);
               }
               printf("Opened the temporary file\n");


               // Create a buffer to hold the file contents
               char buffer[req->wsize];


               // Read the file contents in chunks of size wsize and write each chunk to the sc_pipe
               int sc_fd = open(req->sc_pipe, O_WRONLY);
               if (sc_fd == -1) {
                   perror("open");
                   exit(1);
               } 
               ssize_t bytes_read;
               while ((bytes_read = fread(buffer, 1, req->wsize, temp_file)) > 0) {
                   printf("Read %ld bytes from the temporary file\n", bytes_read);
                   //print the buffer
                   printf("Buffer: %s\n", buffer);
                   //print the size of the buffer
                   printf("Size of buffer: %ld\n", req->wsize);
                   write(sc_fd, buffer, bytes_read);
                   printf("Wrote %ld bytes to the client\n", bytes_read); 
                   if (bytes_read < req->wsize) {
                       break;
                   }
               }


               printf("Sent response to client\n");


               // Check for errors in fread
               if (ferror(temp_file)) {
                   perror("fread");
                   exit(1);
               }


               printf(" after the error check\n");


               // Close the temporary file
               fclose(temp_file);
               printf("Closed the temporary file\n"); */
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
   char *mqname = argv[1];
   char mqname_with_slash[256];
   if (mqname[0] != '/') {
       sprintf(mqname_with_slash, "/%s", mqname);
   } else {
       strncpy(mqname_with_slash, mqname, sizeof(mqname_with_slash) - 1);
   }
   printf("MQNAME: %s\n", mqname);
   printf("MQNAME_WITH_SLASH: %s\n", mqname_with_slash);


   mqd_t mq = mq_open(mqname_with_slash, O_CREAT | O_RDONLY, 0666, NULL);
   if (mq == (mqd_t)-1) {
       perror("mq_open");
       return 1;
   }


   struct mq_attr attr;
   if (mq_getattr(mq, &attr) == -1) {
       perror("mq_getattr");
       return 1;
   }


   char *buffer = malloc(attr.mq_msgsize);
   if (buffer == NULL) {
       perror("malloc");
       return 1;
   }


   while (1) {
       /* // Receive the connection request from the client
       ssize_t bytes_read = mq_receive(mq, buffer, attr.mq_msgsize , NULL);
       if (bytes_read == -1) {
           perror("mq_receive");
           return 1;
       }


       // Cast the received message to a Message struct
       Message *msg = (Message *)buffer;


       // Process the connection request
       int client_pid = 0;
       int wsize = 0;
       char cs_pipe[256];
       char sc_pipe[256];


       // print the data of the message
       printf("Message data: %s\n", msg->data);


       decipher_connection_request(msg, &client_pid, &wsize, cs_pipe, sc_pipe);
       print_message(msg);
       printf("Received connection request from client %d\n", client_pid);


       //check if the pipe names start with a space and remove it
       if (cs_pipe[0] == ' ') {
           memmove(cs_pipe, cs_pipe + 1, strlen(cs_pipe));
       }
       if (sc_pipe[0] == ' ') {
           memmove(sc_pipe, sc_pipe + 1, strlen(sc_pipe));
       }


       ConnectionRequest request = { .client_id = client_pid, .wsize = wsize };
       strncpy(request.cs_pipe, cs_pipe, sizeof(request.cs_pipe) - 1);
       strncpy(request.sc_pipe, sc_pipe, sizeof(request.sc_pipe) - 1);


       //pointer to request named req
       ConnectionRequest *req = &request;


       // print the wsize and client_pid
       printf("wsize: %d\n", wsize);
       printf("client_pid: %d\n", client_pid);
       // print the pipe names from req
       printf("sc_pipe: %s\n", req->sc_pipe);
       printf("cs_pipe: %s\n", req->cs_pipe); */
      
       // Receive the connection request from the client that is sent with the ConnectionRequest struct
       ssize_t bytes_read = mq_receive(mq, buffer, attr.mq_msgsize , NULL);
       if (bytes_read == -1) {
           perror("mq_receive");
           return 1;
       }


       // Cast the received message to a ConnectionRequest struct
       ConnectionRequest *req = (ConnectionRequest *)buffer;


       //print the data of the message received
       printf("Message data: %s\n", req->sc_pipe);
       printf("Message data: %s\n", req->cs_pipe);
       printf("Message data: %d\n", req->client_id);
       printf("Message data: %d\n", req->wsize);


       // Process the connection request
       int client_pid = req->client_id;
       printf("Received connection request from client %d\n", client_pid);


       pid_t pid = fork();
       if (pid == -1) {
           perror("fork");
           free(buffer);
           return 1;
       } else if (pid == 0) {
           printf("Child process created to handle client %d\n", client_pid);
           //print the pipe names
           printf("sc_pipe: %s\n", req->sc_pipe);
           printf("cs_pipe: %s\n", req->cs_pipe);
           handle_client(req);
           exit(0);
       }
   }
   return 0;
}
