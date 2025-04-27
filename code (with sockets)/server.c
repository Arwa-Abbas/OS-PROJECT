#include "communication.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>

#define NUM_THREADS 5    //worker threads in threadpool

#define NUM_RESOURCES 5    

JobQueue *queue = NULL;

// Virtual Memory Global Variables
JobMemory *job_memories[MAX_JOBS] = {NULL};
char phys_mem[NUM_FRAMES][PAGE_SIZE];
int used_frames = 0;

//FIFO REPLACEMENT ALGO
int frame_queue[NUM_FRAMES] = {-1};  
int frame_queue_front=0;
int frame_queue_rear=0;

//READER & WRITER PROBLEM (DEADLOCK)
pthread_rwlock_t queue_rwlock = PTHREAD_RWLOCK_INITIALIZER;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

//Dining Philospher (resources)
pthread_mutex_t printing_resources[NUM_RESOURCES];

sem_t queue_binary_sem;  
sem_t queue_count_sem;

 Resource resources[5] =
    {
    {0, "Print Head", 0, -1},  
    {1, "Ink Cartilage", 0, -1},  
    {2, "Paper Tray", 0, -1},  
    {3, "Power Supply", 0, -1},  
    {4, "Network Interface", 0, -1}  
    };


void init_queue()
{
    queue = (JobQueue*)malloc(sizeof(JobQueue));
    queue->front = 0;                      //circular queue pointers
    queue->rear = 0;
    queue->count = 0;
    queue->current_algorithm=FCFS;         // default algorithm
   
    //synchronization primitives
    sem_init(&queue_binary_sem, 0, 1);
    sem_init(&queue_count_sem, 0, 0);
}

//add new job to queue with thread safe operations
void add_job_to_queue(Job new_job)
{
    sem_wait(&queue_binary_sem);
    printf("Acquiring WRITE lock (add)\n");
    pthread_rwlock_wrlock(&queue_rwlock);
   
    new_job.arrival_time = time(NULL);
   
    if (queue->count == MAX_JOBS)
    {
    printf("Queue full, waiting for space\n");
       pthread_rwlock_unlock(&queue_rwlock);
        sem_post(&queue_binary_sem);
        pthread_cond_wait(&queue_not_full, &queue_mutex);     //wait until space avaliable
        sem_wait(&queue_binary_sem);
        pthread_rwlock_wrlock(&queue_rwlock);
    }
   
    if (queue->current_algorithm == PRIORITY)
    {
        // Insert in priority order (lowest number first)
        int insert_pos = queue->rear;
        for (int i = 0; i < queue->count; i++)
        {
            int idx = (queue->front + i) % MAX_JOBS;
            if (queue->jobs[idx].priority > new_job.priority)
            {
                insert_pos = idx;
                break;
            }
        }
       
        //shift jobs to make space for new jobs
        for (int i = queue->rear; i != insert_pos; i = (i - 1 + MAX_JOBS) % MAX_JOBS)
        {
            int prev = (i - 1 + MAX_JOBS) % MAX_JOBS;
            queue->jobs[i] = queue->jobs[prev];
        }
       
        //insert new job
        queue->jobs[insert_pos] = new_job;
        queue->rear = (queue->rear + 1) % MAX_JOBS;
    }
    else
    {
        queue->jobs[queue->rear] = new_job;
        queue->rear = (queue->rear + 1) % MAX_JOBS;
    }
   
    //update queue state and signal waiting threads
    queue->count++;
    sem_post(&queue_count_sem);
    pthread_rwlock_unlock(&queue_rwlock);
    sem_post(&queue_binary_sem);
    pthread_cond_signal(&queue_not_empty);
   
    //log queue state
    write_queue_to_log();
   
    printf("[SERVER] Added Job %d (Priority: %d, File: %s)\n",
           new_job.jobid, new_job.priority, new_job.filename);
}


//removes and return next job from queue
int remove_job_from_queue(Job *job)
{
    sem_wait(&queue_count_sem);         //waits for available jobs
    printf("LOCKING RWLock (write mode)\n");
     pthread_rwlock_wrlock(&queue_rwlock);

    //handle empty queue condition
    while (queue->count == 0)
    {
        printf("Queue empty, waiting for jobs\n");
         pthread_rwlock_unlock(&queue_rwlock);
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
        pthread_rwlock_wrlock(&queue_rwlock);
    }

    // Default to FCFS if no algorithm is set
    if (queue->current_algorithm == FCFS || queue->current_algorithm == 0)
    {
        *job = queue->jobs[queue->front];
        queue->front = (queue->front + 1) % MAX_JOBS;
        queue->count--;
    }
   
    else if (queue->current_algorithm == PRIORITY)
    {
        // Find the job with highest priority (lowest number)
        int highest_prio_index = queue->front;
        for (int i = 1; i < queue->count; i++)
        {
            int current_index = (queue->front + i) % MAX_JOBS;
            if (queue->jobs[current_index].priority < queue->jobs[highest_prio_index].priority)
            {
                highest_prio_index = current_index;
            }
        }
       
        *job = queue->jobs[highest_prio_index];
       
        for (int i = highest_prio_index; i != queue->rear; i = (i + 1) % MAX_JOBS)
        {
            int next = (i + 1) % MAX_JOBS;
            if (next == queue->rear) break;
            queue->jobs[i] = queue->jobs[next];
        }
       
        queue->rear = (queue->rear - 1 + MAX_JOBS) % MAX_JOBS;
        queue->count--;
    }
   
    pthread_cond_signal(&queue_not_full);       //notify producer of space
    printf("\n[UNLOCKING RWLock (write done)\n");
    pthread_rwlock_unlock(&queue_rwlock);
    return 1;
}

//display current queue log file
void view_log_file()
{
    printf("\n--- CURRENT QUEUE LOG ---\n");
    FILE* log_fp = fopen("queue_log.txt", "r");
   
    if (log_fp)
    {
        char line[256];
        while (fgets(line, sizeof(line), log_fp))
        {
            printf("%s", line);
        }
        fclose(log_fp);
    }
    else
    {
        perror("Error opening queue log file");
    }
    printf("------------------------\n");
}


//worker thread function that process job from queue
void* worker_thread(void* arg)
{
    int thread_id = *(int*)arg;
    Job job;

    while (1)
    {
        remove_job_from_queue(&job);
       
        //resource allocation using dining philospher
        int res1 = thread_id % NUM_RESOURCES;
        int res2 = (thread_id + 1) % NUM_RESOURCES;
       
        // lock lower-numbered resource first to prevent deadlock
        int first = (res1 < res2) ? res1 : res2;
        int second = (res1 < res2) ? res2 : res1;  

        printf("\n[THREAD %d] Attempting Resources %d→%d\n", thread_id, first, second);
       
        pthread_mutex_lock(&printing_resources[first]);
        printf("[THREAD %d] Acquired %s\n", thread_id, resources[first].resource_name);
       
        pthread_mutex_lock(&printing_resources[second]);
        printf("[THREAD %d] Acquired %s - READY\n", thread_id, resources[second].resource_name);

        //update resource allocation status
        resources[first].is_allocated = 1;
        resources[first].job_id = job.jobid;
        resources[second].is_allocated = 1;
        resources[second].job_id = job.jobid;
       
        //handle job memory
        JobMemory *job_mem = job_memories[job.jobid % MAX_JOBS];
         
        if (!job_mem)
        {
            printf("[THREAD %d] ERROR: No memory for Job %d\n", thread_id, job.jobid);
        }
        else
        {
            load_job_pages_into_memory(job, thread_id);  //virtual memory management
 
            printf("\n[THREAD %d] PRINTING JOB %d: %s (Using Resources %s & %s)\n",
                  thread_id, job.jobid, job.filename, resources[first].resource_name, resources[second].resource_name);
            sleep(3);
        }

        //release resources
        pthread_mutex_unlock(&printing_resources[second]);
        pthread_mutex_unlock(&printing_resources[first]);
        printf("\n[THREAD %d] Released %d→%d\n", thread_id, second, first);
             
         if (job.job_type == 1)  // Existing file content received from client
        {
         printf("[THREAD %d] Created a copy of client's existing file: %s\n", thread_id, job.filename);
             printf("[THREAD %d] Content: %s\n", thread_id, job.content);
             FILE *fp = fopen(job.filename, "w");
            if (fp != NULL)
            {
                fprintf(fp, "\n%s\n", job.content);
                fclose(fp);
                printf("[THREAD %d] File %s created successfully.\n", thread_id, job.filename);
            }
            else
            {
                perror("Error creating file");
                printf("[THREAD %d] Error creating file %s.\n", thread_id, job.filename);
            }
                   
        }  
        //new file job      
        else if (job.job_type == 2)
        {
            printf("[THREAD %d] Creating new file: %s\n", thread_id, job.filename);
            printf("[THREAD %d] Heading: %s\n", thread_id, job.heading);
            printf("[THREAD %d] Content: %s\n", thread_id, job.content);
           
            FILE *fp = fopen(job.filename, "w");
            if (fp != NULL)
            {
                fprintf(fp, "%s\n", job.heading);
                fprintf(fp, "\n%s\n", job.content);
                fclose(fp);
                printf("[THREAD %d] File %s created successfully.\n", thread_id, job.filename);
            }
            else
            {
                perror("Error creating file");
                printf("[THREAD %d] Error creating file %s.\n", thread_id, job.filename);
            }
        }  
       
        // Send acknowledgment back to client
        char ack_msg[MSG_SIZE];
        snprintf(ack_msg, MSG_SIZE, "Job %d Completed by Thread %d", job.jobid, thread_id);
        send(job.client_socket, ack_msg, strlen(ack_msg), 0);
       
        sleep(1); // Simulate processing tim
     
        if (strcmp(job.content, "exit") == 0)
        {
            printf("[THREAD %d] Received exit signal. Exiting...\n", thread_id);
            break;
        }
    }
    return NULL;
}


void sort_queue_by_priority()
{
pthread_rwlock_wrlock(&queue_rwlock);
    if (queue->count <= 1) return;

    // Convert circular queue to linear array
    Job temp[MAX_JOBS];
    int temp_count = 0;
   
    // Copy jobs to temp array
    for (int i = 0; i < queue->count; i++)
    {
        int idx = (queue->front + i) % MAX_JOBS;
        temp[temp_count++] = queue->jobs[idx];
    }
   
    // Bubble sort by priority (lower number = higher priority)
    for (int i = 0; i < temp_count - 1; i++)
    {
        for (int j = 0; j < temp_count - i - 1; j++)
        {
            if (temp[j].priority > temp[j+1].priority)
            {
                Job swap = temp[j];
                temp[j] = temp[j+1];
                temp[j+1] = swap;
            }
        }
    }
   
    // Copy back to circular queue
    queue->front = 0;
    queue->rear = temp_count;
    queue->count = temp_count;
    for (int i = 0; i < temp_count; i++)
    {
        queue->jobs[i] = temp[i];
    }
     pthread_rwlock_unlock(&queue_rwlock);
}

//logs current queue status to file
void write_queue_to_log()
{
   printf("\nLOCKING RWLock (read mode)\n");
    pthread_rwlock_rdlock(&queue_rwlock);       //acquire readlock
    FILE* log_fp = fopen("queue_log.txt", "a");
    if (!log_fp)
    {
        perror("Log File Error");
        pthread_mutex_unlock(&queue_mutex);
         sem_post(&queue_binary_sem);
        return;
    }

    //get current timestamp
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    //queue header
    fprintf(log_fp, "\n====== QUEUE STATE AT %s ======\n", time_str);
    fprintf(log_fp, "Algorithm: %s | Total Jobs: %d\n",
           queue->current_algorithm == FCFS ? "FCFS" :
           queue->current_algorithm == PRIORITY ? "Priority" : "Unknown" ,queue->count);

    if (queue->count > 0)
    {
        //job header
        fprintf(log_fp, "Client\tJob ID\tPriority\tFilename\tArrival\t\tStatus\n");
        fprintf(log_fp, "-------------------------------------------------------------------------------\n");
       
        //temp array for sorting if needed
        Job temp_jobs[MAX_JOBS];
        int count = 0;
        for (int i = 0; i < queue->count; i++)
        {
            int idx = (queue->front + i) % MAX_JOBS;
            temp_jobs[count++] = queue->jobs[idx];
        }
       
        if (queue->current_algorithm == PRIORITY)
        {
            for (int i = 0; i < count-1; i++)
            {
                for (int j = 0; j < count-i-1; j++)
                {
                    if (temp_jobs[j].priority > temp_jobs[j+1].priority)
                    {
                        Job swap = temp_jobs[j];
                        temp_jobs[j] = temp_jobs[j+1];
                        temp_jobs[j+1] = swap;
                    }
                }
            }
        }
       
        for (int i = 0; i < count; i++)
        {
            char arrival[30] = "N/A";
            char completion[30] = "N/A";
            char* status = "Pending";
           
            if (temp_jobs[i].arrival_time > 0)
            {
                strftime(arrival, sizeof(arrival), "%H:%M:%S",
                        localtime(&temp_jobs[i].arrival_time));
            }
           
            if (temp_jobs[i].completion_time > 0)
            {
                strftime(completion, sizeof(completion), "%H:%M:%S",
                        localtime(&temp_jobs[i].completion_time));
                status = "Completed";
            }
           
            fprintf(log_fp, "%d\t%d\t%d\t\t%s\t%s\t%s\n",
                   temp_jobs[i].client_socket,
                   temp_jobs[i].jobid,
                   temp_jobs[i].priority,
                   temp_jobs[i].filename,
                   arrival,
                   status);
        }
    }
    else
    {
        fprintf(log_fp, "Queue is empty\n");
    }
   
    fprintf(log_fp, "====== End Update ======\n");
    fclose(log_fp);
      printf("\nUNLOCKING RWLock (read done)\n");
     pthread_rwlock_unlock(&queue_rwlock);
}

//scheduling algorithm for queue
void set_scheduling_algorithm(int algorithm)
{
    pthread_mutex_lock(&queue_mutex);
    queue->current_algorithm = algorithm;
    pthread_mutex_unlock(&queue_mutex);
   
    printf("\nScheduling Algorithm set to: ");
    switch(algorithm)
    {
        case FCFS:
        printf("First-Come-First-Serve\n");
        break;
        case PRIORITY:
        printf("Priority\n");
        break;
        default: printf("Unknown\n");
    }
}

//communication with client
void* handle_client(void* arg)
{
    int client_socket = *(int*)arg;
    struct message msg;
    static int job_id = 1;    //counter for job ids
   
    while (1)
    {
        // Receive message from client
        int bytes_received = recv(client_socket, &msg, sizeof(msg), 0);
        if (bytes_received <= 0)
        {
            printf("Client disconnected\n");
            break;
        }
       
        // parse the received message into a job
        Job new_job;
        new_job.jobid = job_id++;
        new_job.client_socket = client_socket;
        new_job.job_type = msg.job_type;
        new_job.priority = msg.priority;
        strcpy(new_job.filename, msg.mesfilename);
       
    JobMemory *job_mem = malloc(sizeof(JobMemory));
    job_mem->job_id = new_job.jobid;
   
    for (int i = 0; i < PAGES_PER_JOB; i++)
    {
        job_mem->pages[i].frame = -1;  
        job_mem->pages[i].is_modified = 0;
    }
   
    job_memories[new_job.jobid % MAX_JOBS] = job_mem;
    printf("[SERVER] Allocated %d pages for Job %d\n", PAGES_PER_JOB, new_job.jobid);
       
        if (msg.job_type == 1)  // Existing file request
        {
            FILE* fp = fopen(new_job.filename, "r");
            if (fp == NULL)  
            {
                char err_msg[] = "Error: File not found on server.";
                send(client_socket, err_msg, strlen(err_msg), 0);
                continue;
            }

           
            char buffer[MSG_SIZE] = {0};
            fread(buffer, 1, MSG_SIZE - 1, fp);
            fclose(fp);
            send(client_socket, buffer, strlen(buffer), 0);

            // For queue logging
            strcpy(new_job.heading, "EXISTING FILE JOB");
            strcpy(new_job.content, buffer);
        }
        else if (msg.job_type == 2)
        {
            strcpy(new_job.heading, msg.mesheading);
            strcpy(new_job.content, msg.mescontent);
        }
       
        add_job_to_queue(new_job);
       
        printf("\nJob added. View log file? (1 = Yes, 0 = No): ");
        int view_log;
        scanf("%d", &view_log);
        getchar();
       
        if (view_log == 1)
        {
            view_log_file();
        }
        printf("\n📥 [SERVER]: Job Received: ID = %d, Filename = %s\n",new_job.jobid, new_job.filename);
       
       
        if (strcmp(new_job.content, "exit") == 0)
        {
            printf("\n[SERVER]: 'exit' job received from client. Closing connection...\n");
            break;
        }
    }
   
    close(client_socket);
    free(arg);
    return NULL;
}


void start_server()
{
    printf("\nSelect Scheduling Algorithm:\n");
    printf("1. First-Come-First-Serve (FCFS)\n");
    printf("2. Priority\n");
    printf("Enter your choice: ");
    int algorithm_choice;
    scanf("%d", &algorithm_choice);
    set_scheduling_algorithm(algorithm_choice);
   
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
   
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
   
    // Forcefully attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
   
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
   
    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
   
    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
   
    printf("Server listening on port %d...\n", PORT);
   
    // Start worker threads
    pthread_t workers[NUM_THREADS];
    int worker_ids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
    {
        worker_ids[i] = i + 1;
        pthread_create(&workers[i], NULL, worker_thread, &worker_ids[i]);
    }
   
    // Accept incoming connections
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
       
        printf("\nNew connection from %s\n", inet_ntoa(address.sin_addr));
       
        // Create a new thread for each client
        pthread_t thread_id;
        int *client_socket = malloc(sizeof(int));
        *client_socket = new_socket;
       
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_socket) < 0)
        {
            perror("could not create thread");
            continue;
        }
       
        pthread_detach(thread_id);
    }
 
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(workers[i], NULL);
    }
   
    close(server_fd);
}

int main()
{
    printf("\n-------------------------------------------------------------------------------------------\n");
    printf("===========================================================================================\n\n");
    printf("\t\t\t\tMULTI-USER PRINT SERVER");
    printf("\n\n===========================================================================================\n");
    printf("-------------------------------------------------------------------------------------------\n\n");
    printf("\nMADE BY:\n23K-0721\n23K-0700\n23K-0824\n23K-0585\n\n");
    printf("🖨️  Server is now online and ready to receive print jobs.....\n📡  Waiting for client connections...\n\n");
   
    init_queue();
   
    // Initialize printing resources
    for (int i = 0; i < NUM_RESOURCES; i++)
    {
    pthread_mutex_init(&printing_resources[i], NULL);
    }
   
    memset(frame_queue, -1, sizeof(frame_queue));  
    memset(phys_mem, 0, sizeof(phys_mem));     // Clear physical memory
   
    printf("Virtual Memory Ready: %d frames (%d KB total)\n",NUM_FRAMES, (NUM_FRAMES * PAGE_SIZE) / 1024);
    start_server();
   
    // Cleanup printing resources
    for (int i = 0; i < NUM_RESOURCES; i++)
    {
    pthread_mutex_destroy(&printing_resources[i]);
    }

    pthread_rwlock_destroy(&queue_rwlock);
   
    sem_destroy(&queue_binary_sem);
    sem_destroy(&queue_count_sem);
    return 0;
}
