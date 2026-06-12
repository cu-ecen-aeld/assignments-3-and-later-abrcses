#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    struct timespec timesp = {0};
    timesp.tv_nsec = thread_func_args->wait_to_obtain_ms * 1000000;
    int ret = nanosleep(&timesp, NULL);
    if (ret != 0) {
        ERROR_LOG("Error in nanosleep(), returned %d", ret);
        thread_func_args->thread_complete_success = false;
    }

    ret = pthread_mutex_lock(thread_func_args->mutex);
    if (ret != 0) {
        ERROR_LOG("Error in pthread_mutex_lock(), returned %d", ret);
            thread_func_args->thread_complete_success = false;
    } else {
        timesp.tv_nsec = thread_func_args->wait_to_release_ms * 1000000;
        ret = nanosleep(&timesp, NULL);
        if (ret != 0) {
            ERROR_LOG("Error in nanosleep(), returned %d", ret);
            thread_func_args->thread_complete_success = false;
        }

        ret = pthread_mutex_unlock(thread_func_args->mutex);
        if (ret != 0) {
            ERROR_LOG("Error in pthread_mutex_unlock(), returned %d", ret);
            thread_func_args->thread_complete_success = false;
        }
    }

    thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    struct thread_data* thrd_data = (struct thread_data*) malloc(sizeof(struct thread_data));
    thrd_data->mutex = mutex;
    thrd_data->wait_to_obtain_ms = wait_to_obtain_ms;
    thrd_data->wait_to_release_ms = wait_to_release_ms;

    int ret = pthread_create(thread, NULL, threadfunc, thrd_data);
    if (ret != 0) {
        ERROR_LOG("Could not create thread, pthread_create returned %d", ret);
        return false;
    }

    return true;
}

