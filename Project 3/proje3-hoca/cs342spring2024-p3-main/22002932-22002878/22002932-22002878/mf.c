#include "mf.h"

int next_qid = 0; // Next available queue ID
char shmem_name[256]; // Shared memory name
void *shmem_ptr; // Pointer to shared memory
int shmem_size;  // Size of the shared memory
sem_t *sem;      // Semaphore handle
int shm_fd;      // File descriptor for the shared memory

int mf_init() {
    printf("Initializing message flow\n");
    readconfig();

    shm_fd = shm_open(shmem_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, shmem_size) == -1) {
        perror("ftruncate");
        return -1;
    }

    shmem_ptr = mmap(0, shmem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shmem_ptr == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shmem_ptr, shmem_size);
        close(shm_fd);
        return -1;
    }

    return 0;
}

int mf_destroy() {
    printf("Destroying message flow\n");
    if (munmap(shmem_ptr, shmem_size) == -1) {
        perror("munmap");
    }

    if (close(shm_fd) == -1) {
        perror("close");
    }

    if (shm_unlink(shmem_name) == -1) {
        perror("shm_unlink");
    }

    if (sem_close(sem) == -1) {
        perror("sem_close");
    }

    if (sem_unlink(SEM_NAME) == -1) {
        perror("sem_unlink");
    }

    return 0;
}

int mf_connect() {
    printf("Connecting to message flow\n");
    readconfig();

    // Open the shared memory segment
    shm_fd = shm_open(shmem_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    // Map the shared memory segment
    shmem_ptr = mmap(0, shmem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shmem_ptr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shmem_ptr, shmem_size);
        close(shm_fd);
        return -1;
    }

    return 0;  // Success
}

int mf_disconnect() {
    printf("Disconnecting from message flow\n");
    // Unmap the shared memory
    if (munmap(shmem_ptr, shmem_size) == -1) {
        perror("munmap");
        return -1;  // Report failure on error
    }

    // Close the shared memory file descriptor
    if (close(shm_fd) == -1) {
        perror("close");
        return -1;  // Report failure on error
    }

    // Close the semaphore
    if (sem_close(sem) == -1) {
        perror("sem_close");
        return -1;  // Report failure on error
    }

    return 0;  // Success
}

/**int mf_create(char *mqname, int mqsize) {
    printf("Creating message queue %s with size %d\n", mqname, mqsize);
    if (!shmem_ptr) {
        fprintf(stderr, "Error: Shared memory pointer is null.\n");
        return -1;
    }

    int result = -1;  // Default to error return

    do {
        if (next_qid >= MAX_QUEUES) {
            fprintf(stderr, "Error: Maximum number of message queues reached.\n");
            break;  // Early exit to release semaphore
        }

        // Calculate the new queue position in shared memory and check for overflow
        size_t new_queue_pos = sizeof(MessageQueue) * next_qid;
        if (new_queue_pos + sizeof(MessageQueue) > shmem_size) {
            fprintf(stderr, "Error: Not enough shared memory left.\n");
            break;  // Early exit to release semaphore
        }

        MessageQueue *mq = (MessageQueue *)(shmem_ptr + new_queue_pos);
        mq->id = next_qid++;
        strncpy(mq->name, mqname, sizeof(mq->name) - 1);
        mq->name[sizeof(mq->name) - 1] = '\0';  // Ensure null termination
        mq->size = mqsize;
        mq->front = mq->rear = -1;  // Empty queue

        // Successfully created the queue, change result to the queue ID
        result = mq->id;

    } while (0);

    return result;
}*/

int mf_create(char *mqname, int mqsize) {
    if (!shmem_ptr) {
        fprintf(stderr, "Error: Shared memory pointer is null.\n");
        return -1;
    }

    // Lock semaphore for exclusive access
    if (sem_wait(sem) != 0) {
        perror("Semaphore wait failed");
        return -1;
    }

    if (next_qid >= MAX_QUEUES) {  // Assuming MAX_QUEUES is the max number of queues
        fprintf(stderr, "Error: Maximum number of message queues reached.\n");
        sem_post(sem);
        return -1;
    }

    // Calculate the new queue position in shared memory and check for overflow
    size_t new_queue_pos = sizeof(MessageQueue) * next_qid;
    if (new_queue_pos + sizeof(MessageQueue) > shmem_size) {  // Assuming a defined SHARED_MEMORY_SIZE
        fprintf(stderr, "Error: Not enough shared memory left.\n");
        sem_post(sem);
        return -1;
    }

    MessageQueue *mq = (MessageQueue *)(shmem_ptr + new_queue_pos);
    mq->id = next_qid++;
    strncpy(mq->name, mqname, sizeof(mq->name) - 1);
    mq->name[sizeof(mq->name) - 1] = '\0'; // Ensure null termination
    mq->size = mqsize;
    mq->start = new_queue_pos;  // Adjusted to use the actual position
    mq->end = mq->start + mqsize * sizeof(MessageQueue);  // Adjust the size calculation if needed
    mq->front = mq->rear = -1;  // Empty queue

    // Release semaphore
    if (sem_post(sem) != 0) {
        perror("Semaphore wait failed");
        return -1;
    }

    return mq->id;
}



int mf_remove(char *mqname) {
    printf("Removing message queue %s\n", mqname);
    sem_wait(sem);

    MessageQueue *mq = find_queue_by_name(mqname);  // Implement this function
    if (!mq || mq->front != mq->rear) {
        sem_post(sem);
        return -1;  // Queue is either not found or not empty
    }

    // Further clean up logic here, e.g., marking the queue space as free

    sem_post(sem);
    return 0;
}

int mf_open(char *mqname) {
    printf("Opening message queue %s\n", mqname);

    if (!mqname) {
        fprintf(stderr, "Error: Null pointer for mqname provided.\n");
        return -1;
    }

    if (sem_wait(sem) != 0) {
        perror("Semaphore wait failed in mf_open");
        return -1;
    }

    MessageQueue *mq = find_queue_by_name(mqname);
    if (!mq) {
        fprintf(stderr, "Queue named '%s' not found.\n", mqname);
        sem_post(sem);  // Release semaphore even on error
        printf("Semaphore released by mf_open (queue not found)\n");
        return -1;
    }

    if (sem_post(sem) != 0) {
        perror("Semaphore post failed in mf_open");
        return -1;
    }
    return mq->id;
}

int mf_close(int qid) {
    sem_wait(sem);

    MessageQueue *mq = find_queue_by_id(qid);
    if (!mq) {
        sem_post(sem);
        return -1;
    }

    sem_post(sem);
    return 0;
}

int mf_send(int qid, void *bufptr, int datalen) {
    sem_wait(sem);
    MessageQueue *mq = find_queue_by_id(qid);
    if (!mq || !has_space(mq, datalen)) {
        sem_post(sem);
        return -1;
    }

    // Copy data to queue and update indices
    memcpy((char *)mq->shmem_ptr + mq->rear, bufptr, datalen);
    mq->rear = (mq->rear + datalen) % mq->size;

    printf("Sent message: %s\n", (char *)bufptr);

    // Update front index if needed
    if (mq->rear == mq->front) {
        mq->front = (mq->front + 1) % mq->size;
    }

    sem_post(sem);
    return 0;
}

int mf_recv(int qid, void *bufptr, int bufsize) {
    if (sem_wait(sem) != 0) {  // Check for semaphore wait errors
        perror("Semaphore wait failed");
        return -1;  // Return on semaphore error
    }

    MessageQueue *mq = find_queue_by_id(qid);  // Find queue by ID
    if (!mq) {
        fprintf(stderr, "Queue not found\n");
        sem_post(sem);  // Release semaphore on error
        return -1;  // Return error code
    }

    if (is_empty(mq)) {  // Check if queue is empty
        fprintf(stderr, "Queue is empty\n");
        sem_post(sem);  // Release semaphore
        return -1;  // Return error code
    }

    // Cast the shared memory pointer to a char pointer for string operations
    char *msg_start = (char *)shmem_ptr + mq->front;  // Get the start of the message
    int message_length = strlen(msg_start);  // Get the length of the message

    // Check if the buffer is large enough to receive the message
    if (message_length >= bufsize) {  // Prevent buffer overflow
        fprintf(stderr, "Buffer too small to receive message\n");
        sem_post(sem);  // Release semaphore
        return -1;  // Return error code
    }

    // Safely copy the message to the buffer
    memcpy(bufptr, msg_start, message_length);  // Cast bufptr to char pointer

    if (bufsize > message_length) {  // Check if null-termination is possible
        ((char *)bufptr)[message_length] = '\0';  // Null-terminate safely
    }

    // Update the front index in the message queue
    mq->front = (mq->front + message_length) % mq->size;

    // Release the semaphore after successful operations
    if (sem_post(sem) != 0) {
        perror("Semaphore post failed");  // Handle semaphore post errors
        return -1;  // Return error code
    }

    return message_length;  // Return the length of the received message
}

int mf_print() {
    sem_wait(sem);

    // Loop through all queues and print their details
    for (int i = 0; i <= next_qid; ++i) {
        MessageQueue *mq = (MessageQueue *)(shmem_ptr + sizeof(MessageQueue) * i);
        printf("Queue ID: %d, Name: %s, Size: %d\n", mq->id, mq->name, mq->size);
    }

    sem_post(sem);
    return 0;
}

MessageQueue* find_queue_by_name(char *mqname) {
    if (sem_wait(sem) != 0) {  // Acquire the semaphore with error handling
        perror("Semaphore wait failed in find_queue_by_name");
        return NULL;
    }

    printf("Entering find_queue_by_name: searching for %s\n", mqname);
    printf("Next_qid: %d\n", next_qid);

    for (int i = 0; i <= next_qid; ++i) {
        MessageQueue *mq = (MessageQueue *)(shmem_ptr + sizeof(MessageQueue) * i);

        printf("Checking queue %d: %s\n", i, mq->name);
        if (strcmp(mq->name, mqname) == 0) {
            printf("Queue %s found\n", mqname);
            sem_post(sem);  // Release the semaphore
            return mq;
        }
    }

    printf("Queue %s not found, releasing semaphore\n", mqname);
    sem_post(sem);  // Ensure the semaphore is released on error
    return NULL;  // Return NULL if queue is not found
}

MessageQueue *find_queue_by_id(int qid) {
    if (qid < 0 || qid >= next_qid) {  // Correct boundary check
        fprintf(stderr, "Invalid queue ID: %d\n", qid);  // Additional error message
        return NULL;
    }

    // Correct casting and memory alignment
    MessageQueue *mq = (MessageQueue *)(shmem_ptr + sizeof(MessageQueue) * qid);

    return mq;  // Return the found queue
}

int has_space(const MessageQueue *mq, int datalen) {
    int next_rear = (mq->rear + datalen) % mq->size;
    if (mq->front == mq->rear) { // Check if the queue is initially empty
        return 1; // There is always space if the queue is empty
    } else if (mq->front < mq->rear) {
        // Normal condition: Check if adding this data wraps around and overlaps the front
        return (next_rear < mq->rear) && (next_rear < mq->front);
    } else {
        // Wrapped condition: Check if there is enough space between rear and the end or from start to front
        return (next_rear < mq->rear) || (next_rear < mq->front);
    }
}

int is_empty(const MessageQueue *mq) {
    return mq->front == mq->rear;  // True if front and rear are the same
}

void readconfig(){
    const char *config_file = "mf.config";
    FILE *file = fopen(config_file, "r");
    if (!file) {
        perror("Failed to open configuration file");
        exit(EXIT_FAILURE);
    }

    char line[256];

    // Read and parse the configuration file
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || strlen(line) <= 1) {
            // Skip comments and empty lines
            continue;
        }

        char key[256];
        char value[256];
        if (sscanf(line, "%s %s", key, value) == 2) {
            if (strcmp(key, "SHMEM_NAME") == 0) {
                strcpy(shmem_name, value);
            } else if (strcmp(key, "SHMEM_SIZE") == 0) {
                shmem_size = atoi(value);
            }
        }
    }

    fclose(file);
}