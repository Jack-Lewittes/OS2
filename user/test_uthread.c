#include "uthread.h"
#include "user.h"

// Simple function to be used as start function for user threads
void test_func() {
    fprintf(1, "Thread %d running\n", uthread_self()->index);
    uthread_yield();
    fprintf(1, "Thread %d running again\n", uthread_self()->index);
    uthread_exit();
}

void test_uthread_create() {
    // Test creating a thread with valid priority
    int ret = uthread_create(test_func, LOW);
    if (ret != 0) {
        fprintf(1, "Test failed: could not create thread with valid priority\n");
        exit(1);
    }
    // Test creating a thread with invalid priority
    ret = uthread_create(test_func, "invalid priority");
    if (ret == 0) {
        fprintf(1, "Test failed: created thread with invalid priority\n");
        exit(1);
    }
    // Test creating too many threads
    for (int i = 0; i < MAX_UTHREADS; i++) {
        ret = uthread_create(test_func, LOW);
        if (ret != 0) {
            fprintf(1, "Test failed: could not create thread %d\n", i);
            exit(1);
        }
    }
    ret = uthread_create(test_func, LOW);
    if (ret == 0) {
        fprintf(1, "Test failed: created more than MAX_UTHREADS threads\n");
        exit(1);
    }
    fprintf(1, "Test uthread_create passed\n");
}

void test_uthread_yield() {
    // Create three threads with different priorities
    uthread_create(test_func, LOW);
    uthread_create(test_func, MEDIUM);
    uthread_create(test_func, HIGH);
    // Yield twice to make sure the highest priority thread runs first
    uthread_yield();
    uthread_yield();
    // Check that the highest priority thread has run first
    if (uthread_self()->priority != HIGH) {
        fprintf(1, "Test failed: did not run highest priority thread first\n");
        exit(1);
    }
    fprintf(1, "Test uthread_yield passed\n");
}

void test_uthread_exit() {
    // Create two threads and exit the first one
    uthread_create(test_func, LOW);
    uthread_create(test_func, LOW);
    uthread_exit();
    // Check that the first thread has been terminated and the second one is running
    if (uthread_self()->index != 1) {
        fprintf(1, "Test failed: did not exit first thread correctly\n");
        exit(1);
    }
    // Exit the second thread
    uthread_exit();
    // Check that the process has terminated
    if (1) {
        fprintf(1, "Test failed: did not terminate process correctly\n");
        exit(1);
    }
    fprintf(1, "Test uthread_exit passed\n");
}

void test_uthread_set_priority() {
    // Create a thread and set its priority to high
    uthread_create(test_func, LOW);
    uthread_set_priority(HIGH);
    // Check that the priority has been updated
    if (uthread_get_priority() != HIGH) {
        fprintf(2, "Test failed: could not set thread priority correctly\n");
        exit(1);
    }
    // Try to set an invalid priority
    uthread_set_priority(NUM_PRIORITIES+1);
    // Check that the priority has not been updated
    if (uthread_get_priority() != HIGH) {
        fprintf(2, "Test failed: set invalid priority\n");
        exit(1);
    }
    fprintf(1, "Test uthread_set_priority passed\n");
}

void test_uthread_get_priority() {
    // Create a thread with priority MEDIUM
    uthread_create(test_func, MEDIUM);
    // Check that the priority is MEDIUM
    if (uthread_get_priority() != MEDIUM) {
        fprintf(2, "Test failed: could not get thread priority correctly\n");
        exit(1);
    }
    fprintf(1, "Test uthread_get_priority passed\n");
}

int main(int argc, char *argv[]) {
    // Run all tests
    test_uthread_create();
    test_uthread_yield();
    test_uthread_exit();
    test_uthread_set_priority();
    test_uthread_get_priority();
    fprintf(1, "All tests passed!\n");
    exit(0);
}



