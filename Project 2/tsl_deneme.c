#include <stdio.h>
#include <stdlib.h>
#define _XOPEN_SOURCE 700 // Define _XOPEN_SOURCE to resolve the ucontext.h error
#include <ucontext.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

/* We want the extra information from these definitions */
#ifndef __USE_GNU
#define __USE_GNU
#endif /* __USE_GNU */

#include "tsl_deneme.h"

// you will implement your library in this file. 
// you can define your internal structures, macros here. 
// such as: #define ...
// if you wish you can use another header file (not necessary). 
// but you should not change the tsl.h header file. 
// you can also define and use global variables here.
// you can define and use additional functions (as many as you wish) 
// in this file; besides the tsl library functions desribed in the assignment. 
// these additional functions will not be available for 
// applications directly. 

#define TSL_ERROR -1
#define TSL_ANY 0
#define STACK_SIZE 8192

// Thread Control Block (TCB)
enum thread_state {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED,
    THREAD_TERMINATED
};
typedef struct {
    int tid; // Thread ID
    ucontext_t context; // Thread context
    bool deleted; // Flag to mark if thread is deleted
    enum thread_state state; // Current state of the thread
    char* stack; // Pointer to the thread's stack memory
} TCB;


int current_thread_id = -1; // Initialized to an invalid value
int num_threads = 0; // Initialize to 0

// Function prototypes
int tsl_init(int salg);
int tsl_create_thread(void (*tsf)(void *), void *targ);
int tsl_yield(int tid);
int tsl_exit();
int tsl_join(int tid);
int tsl_cancel(int tid);
int tsl_gettid();
void cleanup_thread(TCB* thread);
int enter_critical_section();
int exit_critical_section();

// Global variables
int next_tid = 1; // Next available thread ID
TCB *threads[TSL_MAXTHREADS]; // Array to hold pointers to TCBs
int main_tid; // Main thread ID

static TCB* current;
//static ucontext_t scheduler;

static bool init = false;
//static int id = 0;

// Initialize the thread library
int tsl_init(int salg) {
    // Prevent re-initialization
    if (init) {
        fprintf(stderr, "Thread library already initialized.\n");
        return TSL_ERROR; // Return an error if already initialized
    }
    
    // Mark the library as initialized
    init = true;
    
    // Allocate the main thread TCB and initialize it
    TCB* main_thread = (TCB*)malloc(sizeof(TCB));
    if (!main_thread) {
        perror("Failed to allocate main thread TCB");
        exit(EXIT_FAILURE);
    }
    
    // Get the current context to use as the main thread's context
    //if (getcontext(&main_thread->context) == -1) {
    //    perror("getcontext");
    //    free(main_thread);
    //    exit(EXIT_FAILURE);
    //}
    
    // Initialize the main thread's properties
    main_thread->tid = next_tid++;
    main_thread->state = THREAD_RUNNING;
    main_thread->deleted = false;
    main_thread->stack = NULL; // Main thread will use the process's stack
    
    // Save the main thread in the threads array and set it as the current thread
    threads[main_thread->tid] = main_thread;
    current = main_thread;
    
    // Initialize the scheduler context (optional, depending on your design)
    // You may need to set up the scheduler_context similarly with getcontext and makecontext
    // if you plan to switch to a scheduler function for managing thread execution.
    
    printf("Thread library initialized with main thread ID: %d\n", main_thread->tid);
    
    return TSL_SUCCESS;
}

// Function to create a new thread
int tsl_create_thread(void (*tsf)(void *), void *targ) {
    TCB *thread = (TCB *)malloc(sizeof(TCB));
    if (thread == NULL) {
        return TSL_ERROR; // Memory allocation failed
    }

    // Set up the thread context
    //getcontext(&thread->context);
    thread->context.uc_link = NULL;
    thread->context.uc_stack.ss_sp = malloc(STACK_SIZE);
    thread->context.uc_stack.ss_size = STACK_SIZE;
    thread->tid = next_tid++;
    thread->deleted = false;
    //makecontext(&thread->context, (void (*)(void))tsf, 1, targ);

    threads[thread->tid] = thread; // Add thread to the array
    num_threads++;
    return thread->tid; // Return thread ID
}

// Function to yield CPU to another thread
int tsl_yield(int tid) {
    if (!init) return -1;

    //interrupt_disable();

    // Check if tid is a valid thread id
    if (tid > 0 && tid <= num_threads && threads[tid]->state == THREAD_READY) {
        // Put the current thread back to ready queue
        current->state = THREAD_READY;
        
        // Set the next thread as current
        current = threads[tid];
    } else if (tid == TSL_ANY) {
        // Find the next ready thread to run
        int next_tid = -1;
        for (int i = 1; i <= num_threads; ++i) {
            if (threads[i]->state == THREAD_READY) {
                next_tid = i;
                break;
            }
        }
        
        // If no ready thread found, select the current thread
        if (next_tid == -1)
            next_tid = current->tid;
        
        // Put the current thread back to ready queue
        current->state = THREAD_READY;

        // Set the next thread as current
        current = threads[next_tid];
    } else {
        // Invalid tid, return immediately
        //interrupt_enable();
        return -1;
    }

    // Save the current context and switch to the scheduler
    //if (swapcontext(&current->context, &scheduler) == -1) {
    //    perror("swapcontext to scheduler");
    //    return TSL_ERROR;
    //}
    
    //interrupt_enable();
    return current->tid;
}

// Function to exit the current thread
int tsl_exit() {
    int tid = tsl_gettid(); // ID of the current thread

    if (tid == -1) {
        fprintf(stderr, "Error: Unable to get current thread ID\n");
        exit(EXIT_FAILURE);
    }

    threads[tid]->state = THREAD_TERMINATED;
    current->state = THREAD_TERMINATED; //// CHECK THIS !!!!!!!!

    return 0; // Exited successfully
}


// Function to wait for a thread to finish
int tsl_join(int tid) {
   while (1) {
        int found = 0;
        int completed = 0;

        // Protect access to shared data if necessary
        enter_critical_section(); // Placeholder for entering critical section

        // Check if the target thread is still active
        for (int i = 0; i < TSL_MAXTHREADS; i++) {
            if (threads[i]->tid == tid) {
                found = 1;
                if (threads[i]->state == THREAD_TERMINATED) {
                    completed = 1;
                    // Optionally, perform any cleanup required for the completed thread
                    cleanup_thread(threads[i]);
                }
                break;
            }
        }

        // Exit critical section if implemented
        exit_critical_section();   // Placeholder for exiting critical section

        if (!found) {
            // Target thread not found; return error or handle as appropriate
            return TSL_ERROR; // Assuming TSL_ERROR is defined as an error code
        }

        if (completed) {
            // Target thread has completed; we can return successfully
            return TSL_SUCCESS; // Assuming TSL_SUCCESS is defined as a success code
        }

        // Yield execution to allow other threads, including the target thread, to run
        tsl_yield(TSL_ANY); // Assuming TSL_ANY yields to any other thread
    }
}

void cleanup_thread(TCB* thread) {
    if (thread == NULL) {
        return; // Nothing to clean up  if thread is NULL
    }

    enter_critical_section(); // Placeholder for entering critical section

    // Free the thread's stack memory if it exists
    if (thread->stack != NULL) {
        free(thread->stack);
        thread->stack = NULL; // Avoid dangling pointer
    }

    /*// Inside cleanup_thread()    ///CHECK THIS !!!!!!
    free(thread->context.uc_stack.ss_sp);
    free(thread);*/

    for (int i = 0; i < TSL_MAXTHREADS; i++) {
        if (threads[i]->tid == thread->tid) {
            // Mark the thread as unused in the thread list
            threads[i]->tid = -1; // Assuming -1 indicates an unused or available TCB
            threads[i]->state = THREAD_READY; // Assuming THREAD_UNUSED indicates an available TCB
            break;
        }
    }

    exit_critical_section(); // Placeholder for exiting critical section


    // If your threading model uses other dynamically allocated resources per thread,
    // free those resources here as well.

    // If there's a global or shared data structure tracking active threads,
    // update it to reflect that this thread has been cleaned up.

    // Additional cleanup tasks as necessary...
}

int enter_critical_section() {
    // Placeholder for entering critical section
    return 0; // Placeholder for success
}

int exit_critical_section() {
    // Placeholder for exiting critical section
    return 0; // Placeholder for success
}

// Function to cancel a thread
int tsl_cancel(int tid) {

    if (tid < 0 || tid >= TSL_MAXTHREADS || threads[tid] == NULL) {
        return -1; // Invalid thread ID
    }

    threads[tid]->state = THREAD_BLOCKED;

    return 0; // Canceled successfully
}

// Function to get thread ID
int tsl_gettid() {
    return current->tid;
    
}

// A simple function to be executed by threads
void thread_function(void *arg) {
    int num = *(int *)arg;
    printf("Thread %d starting\n", num);
    
    // Simulate some work by yielding control back and forth
    for (int i = 0; i < 5; ++i) {
        printf("Thread %d working iteration %d\n", num, i + 1);
        tsl_yield(TSL_ANY); // Yield to any other thread
    }

    printf("Thread %d finished\n", num);
    tsl_exit();
}

int main() {
    int salg = 1; // Assuming 1 is a valid scheduling algorithm identifier
    if (tsl_init(salg) != 0) {
        fprintf(stderr, "Failed to initialize threading library\n");
        return 1;
    }

    int thread_args[3] = {1, 2, 3};
    int tids[3];

    // Create three threads
    for (int i = 0; i < 3; ++i) {
        tids[i] = tsl_create_thread(thread_function, &thread_args[i]);
        if (tids[i] < 0) {
            fprintf(stderr, "Failed to create thread %d\n", i + 1);
            return 1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < 3; ++i) {
        if (tsl_join(tids[i]) != 0) {
            fprintf(stderr, "Failed to join thread %d\n", i + 1);
        }
    }

    printf("All threads completed\n");
    return 0;
}