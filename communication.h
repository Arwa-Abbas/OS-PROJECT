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

#define PAGE_SIZE 4096        // 4KB pages
#define PHYS_MEM_SIZE 65536   // 64KB physical memory (16 frames)
#define NUM_FRAMES (PHYS_MEM_SIZE/PAGE_SIZE)
// JOB INFO
#define MAX_JOBS 10
#define MSG_SIZE 512
#define PORT 8080              // Server listening port
#define MAX_CLIENTS 10

// Scheduling algorithms
#define FCFS 1
#define ROUND_ROBIN 2
#define PRIORITY 3


// Page table entry
typedef struct {
    int present;     // Is page in physical memory?
    int frame;       // Frame number if present
    int modified;    // Dirty bit
} PageTableEntry;

// Process memory structure
typedef struct {
    PageTableEntry* page_table;
    int page_count;
    pid_t pid;
} ProcessMemory;

// Physical memory management
typedef struct {
    char frames[NUM_FRAMES][PAGE_SIZE];
    int free_frames[NUM_FRAMES];
    int free_count;
} PhysicalMemory;

extern PhysicalMemory phys_mem;
extern ProcessMemory* processes[MAX_CLIENTS];
extern int process_count;
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
    int execution_time;
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


void init_memory_system();
ProcessMemory* create_process_memory(pid_t pid);
int allocate_page(pid_t pid);
void free_pages(pid_t pid);
char* translate_address(pid_t pid, int virtual_address);
void init_queue();
void add_job_to_queue(Job new_job);
int remove_job_from_queue(Job *job);
void* handle_client(void* arg);
void set_scheduling_algorithm(int algorithm);
void sort_queue_by_priority(void);
void write_queue_to_log();
int count_frames_for_job(int client_socket);
//int job_in_memory(int client_socket, int job_id);
//int get_frame_number(int client_socket, int job_id);

#endif

