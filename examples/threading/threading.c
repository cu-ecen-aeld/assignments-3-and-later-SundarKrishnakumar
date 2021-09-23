#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#define CONVERT_MS_TO_US 1000U

void* threadfunc(void* thread_param)
{

    bool status = true;

    struct thread_data *thread_func_args = (struct thread_data *)thread_param;

    if ( 0 != usleep(thread_func_args->wait_to_obtain_ms * CONVERT_MS_TO_US))
    {
        status = false;
    }
    
    if ( 0 != pthread_mutex_lock(thread_func_args->mutex))
    {
        status = false;
    }

    if ( 0 != usleep(thread_func_args->wait_to_release_ms * CONVERT_MS_TO_US))
    {
        status = false;
    }

    if ( 0 != pthread_mutex_unlock(thread_func_args->mutex))
    {
        status = false;
    }

  
    thread_func_args->thread_complete_success = status;    


    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
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
    struct thread_data *thread_param = malloc(sizeof(struct thread_data));
    int status;

    thread_param->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_param->wait_to_release_ms = wait_to_release_ms;
    thread_param->mutex = mutex;
    thread_param->thread_complete_success = false; // Thread on complettion will set this to true.


    status = pthread_create(thread, NULL, &threadfunc, (void *)thread_param);

    if (status != 0)
    {
        return false;        
    }


    return true;
}

