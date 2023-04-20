#include "uthread.h"
#include "../kernel/types.h"
#include "user.h"

static struct uthread uthreads_table[MAX_UTHREADS];
static struct uthread *current_thread_ptr;
static int num_threads = 0;

void uthread_init() {
    for (int i = 0; i < MAX_UTHREADS; i++) {
        uthreads_table[i].state = FREE;
    }
    num_threads = 0;
}

int uthread_create(void (*start_func)(), enum sched_priority priority) {
    int i;

    // Find a free entry in the table
    for (i = 0; i < MAX_UTHREADS; i++) {
        if (uthreads_table[i].state == FREE) {
            break;
        }
    }
    // Check if there is a free entry
    if (i == MAX_UTHREADS) {
        return -1;
    }
    // Set the state of the new thread to RUNNABLE
    uthreads_table[i].state = RUNNABLE;

    // Initialize the thread's context
    memset(&uthreads_table[i].context, 0, sizeof(struct context));                  // Clear the context
    uthreads_table[i].context.sp = (uint64) &uthreads_table[i].ustack[STACK_SIZE];  // Set the stack pointer
    uthreads_table[i].context.ra = (uint64) start_func;                             // Set the return address

    // Set the thread's priority
    uthreads_table[i].priority = priority;

    // Update the thread's index
    uthreads_table[i].index = i;

    // Update the number of threads
    num_threads++;

    // Set the current thread pointer to the first created thread
    if (num_threads == 1) {
        current_thread_ptr = &uthreads_table[i];
    }

    return 0;
}


void uthread_yield() {
    int i, next_thread = -1;
    enum sched_priority highest_priority = LOW;

    // Find the next runnable thread with highest priority
    for (i = 0; i < MAX_UTHREADS; i++) {
        // Check if the thread is runnable and has higher priority than the current highest priority
        if (uthreads_table[i].state == RUNNABLE && uthreads_table[i].priority > highest_priority) {
            // Update the highest priority and the next thread
            highest_priority = uthreads_table[i].priority;
            next_thread = i;
        }
    }
    // If there is a next thread, switch to it
    if (next_thread != -1) {
        // Save the current context
        uswtch(&current_thread_ptr->context, &uthreads_table[next_thread].context);
        // Update the current thread pointer
        current_thread_ptr = &uthreads_table[next_thread];
    }
}

void uthread_exit() {
    // Free the stack of the terminated thread
    free(current_thread_ptr->ustack);
    // Mark the thread as free
    current_thread_ptr->state = FREE;
    num_threads--;

    if (num_threads == 0) {
        // Last thread, terminate the process
        exit(0);
    } else {
        // Switch to the next runnable thread
        uthread_yield();
    }
}

// Set the priority of the calling user thread to the specified argument and return the previous priority.
enum sched_priority uthread_set_priority(enum sched_priority priority) {
    // Check if the priority is valid
    if (priority < LOW || priority > HIGH) {
        return uthreads_table[current_thread_ptr->index].priority;  // Return current priority
    }
    // Set the new priority
    enum sched_priority prev_priority = uthreads_table[current_thread_ptr->index].priority;
    // Update the priority of the current thread
    uthreads_table[current_thread_ptr->index].priority = priority;
    return prev_priority;
}


// Return the current priority of the calling user thread.
enum sched_priority uthread_get_priority() {
    return current_thread_ptr->priority;
}

/*
Pick the first user thread to run according to the scheduling policy and start it. If successful, this function 
never returns.
If subsequent calls (after the user threads were already started) to uthread_start_all are made, return -1 to 
indicate error.
*/
int uthread_start_all() {
    static int started = 0;
    if (started) {
        return -1;
    }
    started = 1;
    while (num_threads > 0) {
        uthread_yield();
    }
    // No more threads to run, terminate the process
    exit(0);
}

// Return a pointer to the UTCB associated with the calling thread.
struct uthread* uthread_self() {
    return current_thread_ptr;
}




/*
THREAD CONTEXT SWITCHES EXPLAINED:

the context switch between threads is implemented using assembly code in the files swtch.S and uswtch.S. 
The swtch() function in swtch.S is responsible for saving the current thread's context and restoring the context of
the next thread to run. The uswtch() function in uswtch.S is a variant of swtch() that is used specifically for 
switching between user-level threads.

The context switch itself is performed using a combination of stack manipulation and register state changes. 
When a thread yields, the current state of the thread's registers is saved on its stack. This includes the program 
counter (PC), stack pointer (SP), and other registers that contain important state information. The stack pointer is 
then updated to point to the stack of the next thread to run, and the state of its registers is restored from its saved 
context. Control is then transferred to the new thread, and it begins executing from the point where it left off.

*/