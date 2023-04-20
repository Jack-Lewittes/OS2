// #include "uthread.h"
// #include "uthread.c"
// #include <stdio.h>
// #include <stdlib.h>
// #include <assert.h>

// #define NUM_THREADS 2

// void start_func_1();
// void start_func_2();

// struct uthread threads[NUM_THREADS];
// int thread_count = 0;

// void test_uthread_create() {
//   // Create a new thread with priority HIGH
//   int result = uthread_create(start_func_1, HIGH);
//   assert(result == 0);
  
//   // Create a new thread with priority LOW
//   result = uthread_create(start_func_2, LOW);
//   assert(result == 0);
// }

// void test_uthread_set_priority() {
//   // Set the priority of the first thread to LOW
//   enum sched_priority prev_priority = uthread_set_priority(LOW);
//   assert(prev_priority == HIGH);
  
//   // Set the priority of the second thread to HIGH
//   prev_priority = uthread_set_priority(HIGH);
//   assert(prev_priority == LOW);
// }

// void test_uthread_get_priority() {
//   // Get the priority of the first thread
//   enum sched_priority priority = uthread_get_priority();
//   assert(priority == LOW);
  
//   // Get the priority of the second thread
//   priority = uthread_get_priority();
//   assert(priority == HIGH);
// }

// void test_uthread_yield() {
//   // Switch to the second thread
//   uthread_yield();
  
//   // Switch back to the first thread
//   uthread_yield();
// }

// void test_uthread_exit() {
//   // Exit the first thread
//   uthread_exit();
  
//   // The second thread should now be running
//   // Exit the second thread
//   uthread_exit();
  
//   // There should be no more running threads
// }

// void test_uthread_start_all() {
//   // Try to start all threads from the main thread
//   int result = uthread_start_all();
//   assert(result == -1);
// }

// void test_uthread_self() {
//   // Get the UTCB of the first thread
//   struct uthread* thread = uthread_self();
//   assert(thread == &threads[0]);
  
//   // Get the UTCB of the second thread
//   thread = uthread_self();
//   assert(thread == &threads[1]);
// }

// int main() {
//   // Initialize the uthread library
//   uthread_init();

//   // Run all tests
//   test_uthread_create();
//   test_uthread_set_priority();
//   test_uthread_get_priority();
//   test_uthread_yield();
//   test_uthread_exit();
//   test_uthread_start_all();
//   test_uthread_self();
  
//   printf("All tests passed!\n");

//   // Clean up the uthread library
//   uthread_cleanup();

//   return 0;
// }

// void start_func_1() {
//   printf("Thread 1 running...\n");
//   uthread_exit();
// }

// void start_func_2() {
//   printf("Thread 2 running...\n");
//   uthread_exit();
// }
