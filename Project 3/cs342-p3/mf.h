#ifndef MF_H
#define MF_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#define SHMEM_NAME "mf_shmem"
#define SHMEM_SIZE 1024 * 1024  // 1MB
#define SEM_NAME "mf_semaphore"


extern int next_qid; // Next available queue ID
extern void *shmem_ptr; // Pointer to shared memory
extern int shmem_size;  // Size of the shared memory
extern sem_t *sem;      // Semaphore handle
extern int shm_fd;      // File descriptor for the shared memory

typedef struct {
    int id;
    int size;
    char name[256];
    int start;  // Offset in the shared memory where the queue starts
    int end;    // Offset in the shared memory where the queue ends
    int front;  // Index for the front of the queue
    int rear;   // Index for the rear of the queue
    void *shmem_ptr;
} MessageQueue;

// Function prototypes
int mf_init();
int mf_destroy();
int mf_connect();
int mf_disconnect();
int mf_create(char *mqname, int mqsize);
int mf_remove(char *mqname);
int mf_open(char *mqname);
int mf_close(int qid);
int mf_send(int qid, void *bufptr, int datalen);
int mf_recv(int qid, void *bufptr, int bufsize);
int mf_print();
int has_space(const MessageQueue *mq, int datalen);
int is_empty(const MessageQueue *mq);


MessageQueue* find_queue_by_name(char *mqname);
MessageQueue *find_queue_by_id(int qid);
#endif
