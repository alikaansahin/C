#include "mf.h"

int next_qid = 0; // Next available queue ID
void *shmem_ptr; // Pointer to shared memory
int shmem_size;  // Size of the shared memory
sem_t *sem;      // Semaphore handle
int shm_fd;      // File descriptor for the shared memory

int mf_init() {
    char *shmem_name = SHMEM_NAME; // Assuming default, change if reading from config
    shm_fd = shm_open(shmem_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, SHMEM_SIZE) == -1) {
        perror("ftruncate");
        return -1;
    }

    shmem_ptr = mmap(0, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shmem_ptr == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shmem_ptr, SHMEM_SIZE);
        close(shm_fd);
        return -1;
    }

    return 0;
}

int mf_destroy() {
    if (munmap(shmem_ptr, SHMEM_SIZE) == -1) {
        perror("munmap");
    }

    if (close(shm_fd) == -1) {
        perror("close");
    }

    if (shm_unlink(SHMEM_NAME) == -1) {
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
    char *config_file = "mf.config";
    FILE *_file = fopen(config_file, "r");
    if (!config_file) {
        perror("Failed to open configuration file");
        return -1;
    }

    char shmem_name[256];
    int shmem_size = 0;
    char line[256];

    // Read and parse the configuration file
    while (fgets(line, sizeof(line), config_file)) {
        if (sscanf(line, "SHMEM_NAME=%s", shmem_name) == 1) {
            continue;
        } else if (sscanf(line, "SHMEM_SIZE=%d", &shmem_size) == 1) {
            continue;
        }
    }

    fclose(config_file);

    // Open the shared memory segment
    shm_fd = shm_open(shmem_name, O_RDWR, 0666);
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

    // Assuming semaphore is already created by mf_init
    sem = sem_open(SEM_NAME, 0);  // Open existing semaphore
    if (sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shmem_ptr, shmem_size);
        close(shm_fd);
        return -1;
    }

    // Store or register this data as needed by your library
    // This might involve setting global or thread-specific data

    return 0;  // Success
}

int mf_disconnect() {
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
    if (new_queue_pos + sizeof(MessageQueue) > SHARED_MEMORY_SIZE) {  // Assuming a defined SHARED_MEMORY_SIZE
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
    if (!mqname) {
        fprintf(stderr, "Error: Null pointer for mqname provided.\n");
        return -1;
    }

    printf("Requesting to open queue: %s\n", mqname);

    if (sem_wait(sem) != 0) {
        perror("Semaphore wait failed in mf_open");
        return -1;
    }
    printf("Semaphore acquired by mf_open\n");

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
    printf("Semaphore released by mf_open\n");
    return mq->id;
}




int mf_close(int qid) {
    sem_wait(sem);

    MessageQueue *mq = find_queue_by_id(qid);  // Implement this function
    if (!mq) {
        sem_post(sem);
        return -1;
    }

    // Decrement reference count and handle clean up if it reaches zero

    sem_post(sem);
    return 0;
}

int mf_send(int qid, void *bufptr, int datalen) {
    sem_wait(sem);
    MessageQueue *mq = find_queue_by_id(qid);
    if (!mq || !has_space(mq, datalen)) {  // Implement has_space to check available space
        sem_post(sem);
        return -1;
    }

    // Copy data to queue and update indices

    sem_post(sem);
    return 0;
}

int mf_recv(int qid, void *bufptr, int bufsize) {
    sem_wait(sem);
    MessageQueue *mq = find_queue_by_id(qid);
    if (!mq) {
        fprintf(stderr, "Queue not found\n");
        sem_post(sem);
        return -1;
    }

    if (is_empty(mq)) {
        fprintf(stderr, "Queue is empty\n");
        sem_post(sem);
        return -1;
    }

    // Assuming messages are stored as strings for simplicity
    int message_length = strlen((char *)mq->shmem_ptr + mq->front);
    if (message_length > bufsize) {
        fprintf(stderr, "Buffer too small to receive message\n");
        sem_post(sem);
        return -1;
    }

    memcpy(bufptr, (char *)mq->shmem_ptr + mq->front, message_length);
    printf("Received message: %s\n", (char *)bufptr);

    mq->front = (mq->front + message_length) % mq->size;  // Move front past the message just read
    sem_post(sem);
    return message_length;
}


int mf_print() {
    sem_wait(sem);

    // Loop through all queues and print their details
    for (int i = 0; i < next_qid; ++i) {
        MessageQueue *mq = (MessageQueue *)(shmem_ptr + sizeof(MessageQueue) * i);
        printf("Queue ID: %d, Name: %s, Size: %d\n", mq->id, mq->name, mq->size);
    }

    sem_post(sem);
    return 0;
}

MessageQueue* find_queue_by_name(char *mqname) {
    printf("Entering find_queue_by_name: searching for %s\n", mqname);
    printf("Semaphore acquired in find_queue_by_name\n");

    for (int i = 0; i < next_qid; ++i) {
        MessageQueue *mq = (MessageQueue *)(shmem_ptr + sizeof(MessageQueue) * i);
        printf("Checking queue %d: %s\n", i, mq->name);
        if (strcmp(mq->name, mqname) == 0) {
            printf("Queue %s found\n", mqname);
            sem_post(sem);
            return mq;
        }
    }

    printf("Queue %s not found, releasing semaphore\n", mqname);
    return NULL;
}



MessageQueue *find_queue_by_id(int qid) {
    if (qid < 0 || qid >= next_qid) {
        return NULL;
    }

    return (MessageQueue *)(shmem_ptr + sizeof(MessageQueue) * qid);
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
