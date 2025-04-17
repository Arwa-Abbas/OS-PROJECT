#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>        // Socket functions
#include <netinet/in.h>        // sockaddr_in structure
#include <arpa/inet.h>

// JOB INFO
#define MAX_JOBS 10
#define MSG_SIZE 512
#define PORT 8080              // Server listening port
#define MAX_CLIENTS 10

// Message structure
struct message 
{
    long mestype;
    char mesfilename[MSG_SIZE];
    char mesheading[MSG_SIZE];
    char mescontent[MSG_SIZE];
};

// Job structure
typedef struct 
{
    int jobid;
    char filename[MSG_SIZE];
    char heading[MSG_SIZE];
    char content[MSG_SIZE];
    int client_socket;            // To track which client sent the job
} Job;

// Job Queue
typedef struct 
{
    Job jobs[MAX_JOBS];
    int front, rear, count;
} JobQueue;

extern JobQueue *queue;
extern pthread_mutex_t queue_mutex;
extern pthread_cond_t queue_not_empty;
extern pthread_cond_t queue_not_full;

void init_queue();
int add_job_to_queue(Job new_job);
int remove_job_from_queue(Job *job);
void* handle_client(void* arg);

#endif
