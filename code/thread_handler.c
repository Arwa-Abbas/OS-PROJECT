// need to modify this for proper implementation of multithreading (threadpool)

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "thread_handler.h"

#define THREAD_COUNT 3

static pthread_t threads[THREAD_COUNT];
static JobQueue *sharedQueue;
static int semid_global;
static int running = 1;

void sem_wait_custom(int sem_num) 
{
    struct sembuf sb={sem_num, -1, 0};
    semop(semid_global, &sb, 1);
}

void sem_signal_custom(int sem_num) 
{
    struct sembuf sb={sem_num, 1, 0};
    semop(semid_global, &sb, 1);
}

void* thread_function(void* arg) 
{
    while (running) 
    {
        sem_wait_custom(FULL);
        sem_wait_custom(MUTEX);

        Job job = sharedQueue->jobs[sharedQueue->front];
        sharedQueue->front = (sharedQueue->front + 1) % MAX_JOBS;
        sharedQueue->count--;

        sem_signal_custom(MUTEX);
        sem_signal_custom(EMPTY);

        if (strcmp(job.content, "exit") == 0) 
        {
            printf("[THREAD %ld]: Exit job received.\n", pthread_self());
            running = 0;
            break;
        }

        printf("[THREAD %ld]: Processing job ID %d | Content: %s\n", pthread_self(), job.jobid, job.content);
        sleep(2); 
        printf("[THREAD %ld]: Finished job ID %d\n", pthread_self(), job.jobid);
    }

    return NULL;
}

void start_thread_pool(JobQueue *queue, int semid) 
{
    sharedQueue = queue;
    semid_global = semid;
    running = 1;

    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        pthread_create(&threads[i], NULL, thread_function, NULL);
    }
    printf("[THREAD_HANDLER]: Started %d threads.\n", THREAD_COUNT);
}

void stop_thread_pool()
{
    for (int i = 0; i < THREAD_COUNT; ++i) 
      {   
        pthread_join(threads[i], NULL);
    }

    // Push the exit job into the queue
    sem_wait_custom(EMPTY);
    sem_wait_custom(MUTEX);
    sharedQueue->jobs[sharedQueue->rear] = exitJob;
    sharedQueue->rear = (sharedQueue->rear + 1) % MAX_JOBS;
    sharedQueue->count++;
    sem_signal_custom(MUTEX);
    sem_signal_custom(FULL);

    
    printf("[THREAD_HANDLER]: All threads exited.\n");
}

