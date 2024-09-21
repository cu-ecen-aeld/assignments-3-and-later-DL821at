#include "threading.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // Obtain thread arguments from the parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    // Attempt to obtain the mutex
    if (pthread_mutex_lock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to obtain mutex");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    // Simulate a task being performed while holding the mutex
    // For example, this could be resource-intensive work or a critical section
    for (int i = 0; i < thread_func_args->wait_to_release_ms; ++i) {
        // Placeholder for work being done
        // No explicit sleep, just simulation of work through looping
    }

    // Release the mutex after the task is complete
    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to release mutex");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    // Mark the thread as successfully completed
    thread_func_args->thread_complete_success = true;

    // Return the thread data structure
    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    // Allocate memory for thread_data
    struct thread_data *data = (struct thread_data *)malloc(sizeof(struct thread_data));
    if (data == NULL) {
        ERROR_LOG("Failed to allocate memory for thread data");
        return false;
    }

    // Set up the thread data
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;

    // Here we could perform some logic to simulate waiting before locking the mutex.
    // For simplicity, we skip this and focus on mutex logic.

    // Create the thread
    if (pthread_create(thread, NULL, threadfunc, (void *)data) != 0) {
        ERROR_LOG("Failed to create thread");
        free(data);
        return false;
    }

    return true;
}
