
#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>          // socket functions
#include <netinet/in.h>          // sockaddr_in structure
#include <arpa/inet.h>

#define NUM_FRAMES 8        
#define PAGE_SIZE 4096         // 4KB per page 
#define PAGES_PER_JOB 4  

// job info
#define MAX_JOBS 20
#define MSG_SIZE 512
#define PORT 8080               // server listening port
#define MAX_CLIENTS 5

// scheduling algorithms
#define FCFS 1
#define PRIORITY 2


// page table entry
typedef struct 
{
    int frame;            // physical frame number (-1 if not in memory)
    int is_used;          // 1 if page is allocated, 0 if it is free
    int is_modified;      // for tracking if whether data was changed
} PageTableEntry;


typedef struct 
{
PageTableEntry pages[PAGES_PER_JOB];     
    int job_id;                           // Links to Job struct
    int client_socket;                    
} JobMemory;


extern JobMemory *job_memories[MAX_JOBS];        // maps job_id to the memory
extern char phys_mem[NUM_FRAMES][PAGE_SIZE];     // physical memory
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

// job structure
typedef struct
{
    int jobid;
    char filename[MSG_SIZE];
    char heading[MSG_SIZE];
    char content[MSG_SIZE];
    int client_socket;            // To track which client sent the job
    int job_type;
    int priority;                 // priority for scheduling
    time_t arrival_time;    
    time_t completion_time;
} Job;


// Job Queue
typedef struct
{
    Job jobs[MAX_JOBS];
    int front, rear, count;
     int current_algorithm;
} JobQueue;

// resources
typedef struct
{
    int resource_id;            
    char resource_name[MSG_SIZE]; 
    int is_allocated;         
    int job_id;                
} Resource;



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
