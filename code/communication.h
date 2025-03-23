#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>       //for ipc
#include <sys/msg.h>       //for message queue
#include <sys/shm.h>       // for shared memory
#include <sys/sem.h>       // for semaphores
#include <sys/types.h>

//JOB INFO
#define MAX_JOBS 10
#define MSG_SIZE 256

// Keys
#define SHM_KEY 1234            // for shared memory
#define SEM_KEY 5678            // for semaphores
#define MSG_KEY 9012            // for message queue

// Semaphore indices (three defined)
#define MUTEX 0
#define FULL  1
#define EMPTY 2
#define NUM_SEMAPHORES 3

// Message structure
struct message 
{
    long mestype;
    char mestext[MSG_SIZE];
};

// Job structure
typedef struct 
{
    int jobid;
    char content[MSG_SIZE];
} Job;

// Job Queue
typedef struct 
{
    Job jobs[MAX_JOBS];
    int front,rear,count;
} JobQueue;

#endif
