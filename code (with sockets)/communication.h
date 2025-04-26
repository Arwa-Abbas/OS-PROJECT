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

#define NUM_FRAMES 8        
#define PAGE_SIZE 4096        // 4KB per page
#define PAGES_PER_JOB 4  

// JOB INFO
#define MAX_JOBS 2
#define MSG_SIZE 512
#define PORT 8080              // Server listening port
#define MAX_CLIENTS 10

// Scheduling algorithms
#define FCFS 1
#define ROUND_ROBIN 2
#define PRIORITY 3


// Page table entry
typedef struct
{
    int frame;          // Physical frame number (-1 if not in memory)
    int is_used;        // 1 if page is allocated, 0 if free
    int is_modified;    // For tracking if data was changed
} PageTableEntry;


typedef struct
{
PageTableEntry pages[PAGES_PER_JOB];  // Each job has fixed pages
    int job_id;                           // Links to Job struct
    int client_socket;                    // Which client owns this  
} JobMemory;

extern JobMemory *job_memories[MAX_JOBS];  // Maps job_id → memory
extern char phys_mem[NUM_FRAMES][PAGE_SIZE]; // Physical memory
extern int used_frames;  


extern int frame_queue[NUM_FRAMES];  
extern int frame_queue_front;
extern int frame_queue_rear;
extern int used_frames;

// Message structure
struct message
{
    long mestype;
    char mesfilename[MSG_SIZE];
    char mesheading[MSG_SIZE];
    char mescontent[MSG_SIZE];
    int job_type;
    int priority;  
};


// Job structure
typedef struct
{
    int jobid;
    char filename[MSG_SIZE];
    char heading[MSG_SIZE];
    char content[MSG_SIZE];
    int client_socket;            // To track which client sent the job
    int job_type;
    int priority;                 // Priority for scheduling
    time_t arrival_time;    
    time_t completion_time;
} Job;


// Job Queue
typedef struct
{
    Job jobs[MAX_JOBS];
    int front, rear, count;
     int current_algorithm;
    int rr_counter;
} JobQueue;


extern JobQueue *queue;
extern pthread_mutex_t queue_mutex;
extern pthread_cond_t queue_not_empty;
extern pthread_cond_t queue_not_full;

void init_queue();
void add_job_to_queue(Job new_job);
int remove_job_from_queue(Job *job);
void* handle_client(void* arg);
void set_scheduling_algorithm(int algorithm);
void sort_queue_by_priority(void);
void write_queue_to_log();
void load_job_pages_into_memory(Job job, int thread_id);


#endif
