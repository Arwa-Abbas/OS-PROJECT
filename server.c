#include "communication.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

#define NUM_THREADS 4

extern ProcessMemory* processes[MAX_CLIENTS];
extern int process_count;
JobQueue *queue = NULL;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

void init_queue()
{
    queue = (JobQueue*)malloc(sizeof(JobQueue));
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->current_algorithm = FCFS;         // default algorithm
    queue->rr_counter = 0;
}

void add_job_to_queue(Job new_job)
{
    pthread_mutex_lock(&queue_mutex);
    new_job.arrival_time = time(NULL);
    if (queue->count == MAX_JOBS)
    {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
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
       
        for (int i = queue->rear; i != insert_pos; i = (i - 1 + MAX_JOBS) % MAX_JOBS)
        {
            int prev = (i - 1 + MAX_JOBS) % MAX_JOBS;
            queue->jobs[i] = queue->jobs[prev];
        }
       
        queue->jobs[insert_pos] = new_job;
        queue->rear = (queue->rear + 1) % MAX_JOBS;
    }
    else
    {
        // For FCFS and Round Robin, add to rear
        queue->jobs[queue->rear] = new_job;
        queue->rear = (queue->rear + 1) % MAX_JOBS;
    }
   
    queue->count++;
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
   
    write_queue_to_log();
   
    printf("[SERVER] Added Job %d (Priority: %d, File: %s)\n",
           new_job.jobid, new_job.priority, new_job.filename);
}



int remove_job_from_queue(Job *job)
{
    pthread_mutex_lock(&queue_mutex);
    while (queue->count == 0)
    {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
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
   
    else if (queue->current_algorithm == ROUND_ROBIN)
    {
        *job = queue->jobs[queue->front];
        queue->front = (queue->front + 1) % MAX_JOBS;
        queue->count--;
    }

    pthread_cond_signal(&queue_not_full);
    pthread_mutex_unlock(&queue_mutex);
    return 1;
}

void view_log_file()
{
    printf("\n--- Current Queue Log ---\n");
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
        perror("Error opening log file");
    }
    printf("------------------------\n");
}


void* worker_thread(void* arg)
{
    int thread_id = *(int*)arg;
    Job job;
    time_t start = time(NULL);
   
   while (1)
    {
        remove_job_from_queue(&job);
        int pages_needed = (strlen(job.content) / PAGE_SIZE) + 1;
        printf("[THREAD %d] Processing Job %d (Requires %d pages)\n",
               thread_id, job.jobid, pages_needed);
       for (int i = 0; i < pages_needed; i++) {
            int frame = allocate_page(job.client_socket);
            if (frame >= 0) {
               // char* phys_addr = translate_address(job.client_socket, frame * PAGE_SIZE);
        for (int p = 0; p < pages_needed; p++) {
            int page = allocate_page(job.client_socket);
            if (page >= 0) {
                char* frame = translate_address(job.client_socket, page*PAGE_SIZE);
                printf("[THREAD %d] Allocated page %d (frame %d) for '%s'\n",
                       thread_id, page, page/PAGE_SIZE, job.filename);
               
                // Copy portion of content to this page
                strncpy(frame, job.content + p*PAGE_SIZE, PAGE_SIZE);
            }
         }
       }
      }
        printf("[THREAD %d] Processing Job ID = %d | Filename: %s \n\n",
         thread_id, job.jobid, job.filename);
         
        // Allocate memory for this job
        int virtual_address = allocate_page(job.client_socket);
        if (virtual_address >= 0) {
            char* phys_addr = translate_address(job.client_socket, virtual_address);
            if (phys_addr) {
                // Store job content in allocated page
                strncpy(phys_addr, job.content, PAGE_SIZE);
               
                // Mark page as modified
                for (int i = 0; i < process_count; i++) {
                    if (processes[i]->pid == job.client_socket) {
                        int page_num = virtual_address / PAGE_SIZE;
                        processes[i]->page_table[page_num].modified = 1;
                        break;
                    }
                }
            }
        }
       
        /*if (job.job_type == 1)  // Existing file read job
        {
            printf("[THREAD %d] Existing file requested\n", thread_id);
            printf("[THREAD %d] Content: %s\n", thread_id, job.content);
        }*/
       
         if (job.job_type == 1)  // Existing file content received from client
        {
         printf("[THREAD %d] Created a copy of client file: %s\n", thread_id, job.filename);
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
       
        sleep(1); // Simulate processing time
        time_t end = time(NULL);
job.completion_time = end;
job.execution_time = (int)difftime(end, start);
       
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
}

void write_queue_to_log()
{
    pthread_mutex_lock(&queue_mutex);
   
    FILE* log_fp = fopen("queue_log.txt", "a");
    if (!log_fp)
    {
        perror("Log File Error");
        pthread_mutex_unlock(&queue_mutex);
        return;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
     /* ====== NEW MEMORY TRACKING OUTPUT ====== */
    fprintf(log_fp, "\n====== Queue State at %s ======\n", time_str);
    fprintf(log_fp, "Memory Status: %d/%d frames used\n",
            NUM_FRAMES - phys_mem.free_count, NUM_FRAMES);
    fprintf(log_fp, "Algorithm: %s | Total Jobs: %d\n",
           queue->current_algorithm == FCFS ? "FCFS" :
           queue->current_algorithm == PRIORITY ? "Priority" : "Round Robin",
           queue->count);

    if (queue->count > 0) {
        fprintf(log_fp, "Client\tJob ID\tPriority\tFrames\tFilename\tStatus\n");
        fprintf(log_fp, "--------------------------------------------------\n");
       
        for (int i = 0; i < queue->count; i++) {
            int idx = (queue->front + i) % MAX_JOBS;
            Job job = queue->jobs[idx];
           
            int frames_used = count_frames_for_job(job.client_socket);
            //(void)job_id;
            char* status = frames_used > 0 ? "In Memory" : "Queued";
           
            fprintf(log_fp, "%d\t%d\t%d\t\t%d\t%s\t%s\n",
                   job.client_socket,
                   job.jobid,
                   job.priority,
                   frames_used,
                   job.filename,
                   status);
        }
    } else {
        fprintf(log_fp, "Queue is empty\n");
    }
    /* ====== END NEW OUTPUT ====== */


    fprintf(log_fp, "\n====== Queue State at %s ======\n", time_str);
    fprintf(log_fp, "Memory Status: %d/%d frames used\n",
            NUM_FRAMES - phys_mem.free_count, NUM_FRAMES);
    fprintf(log_fp, "Algorithm: %s | Total Jobs: %d\n",
           queue->current_algorithm == FCFS ? "FCFS" :
           queue->current_algorithm == PRIORITY ? "Priority" : "Round Robin",
           queue->count);
    /*fprintf(log_fp, "\n====== Queue State at %s ======\n", time_str);
    fprintf(log_fp, "Algorithm: %s | Total Jobs: %d\n",
           queue->current_algorithm == FCFS ? "FCFS" :
           queue->current_algorithm == PRIORITY ? "Priority" : "Round Robin",
           queue->count);*/

    if (queue->count > 0)
    {
        fprintf(log_fp, "Client\tJob ID\tPriority\tFilename\tArrival\t\tCompletion\t\n");
        fprintf(log_fp, "-------------------------------------------------------------------------------\n");
       
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
            char completion[30] = "Pending";
            //int turnaround_time = -1;
            //int waiting_time = -1;
           
            if (temp_jobs[i].arrival_time > 0)
            {
                strftime(arrival, sizeof(arrival), "%H:%M:%S",
                        localtime(&temp_jobs[i].arrival_time));
               
                if (temp_jobs[i].completion_time > 0)
                {
                    strftime(completion, sizeof(completion), "%H:%M:%S",
                            localtime(&temp_jobs[i].completion_time));
                }
            }
           
            fprintf(log_fp, "%d\t%d\t%d\t\t%s\t%s\t%s\n",
                   temp_jobs[i].client_socket,
                   temp_jobs[i].jobid,
                   temp_jobs[i].priority,
                   temp_jobs[i].filename,
                   arrival,
                   completion);
        }
    }
    else
    {
        fprintf(log_fp, "Queue is empty\n");
    }
   
    fprintf(log_fp, "====== End Update ======\n");
    fclose(log_fp);
    pthread_mutex_unlock(&queue_mutex);
}


void set_scheduling_algorithm(int algorithm)
{
    pthread_mutex_lock(&queue_mutex);
    queue->current_algorithm = algorithm;
    queue->rr_counter = 0;
    pthread_mutex_unlock(&queue_mutex);
   
    printf("\nScheduling Algorithm set to: ");
    switch(algorithm)
    {
        case FCFS:
        printf("First-Come-First-Serve\n");
        break;
        case ROUND_ROBIN:
        printf("Round Robin\n");
        break;
        case PRIORITY:
        printf("Priority\n");
        break;
        default: printf("Unknown\n");
    }
}

void* handle_client(void* arg)
{
    int client_socket = *(int*)arg;
    //ProcessMemory* proc = create_process_memory(client_socket);
    struct message msg;
    int job_id = 1;
   
    while (1)
    {
        // Receive message from client
        int bytes_received = recv(client_socket, &msg, sizeof(msg), 0);
        if (bytes_received <= 0)
        {
            printf("Client disconnected\n");
            break;
        }
       
        // Parse the received message into a job
        Job new_job;
        new_job.jobid = job_id++;
        new_job.client_socket = client_socket;
        new_job.job_type = msg.job_type;
        new_job.priority = msg.priority;
        strcpy(new_job.filename, msg.mesfilename);
       
        if (msg.job_type == 1)  // Existing file request
        {
            FILE* fp = fopen(new_job.filename, "r");
            if (fp == NULL)  // Fixed: Changed 'ioif' to 'if'
            {
                char err_msg[] = "Error: File not found on server.";
                send(client_socket, err_msg, strlen(err_msg), 0);
                continue;
            }

            // Read and send file content back to client
            char buffer[MSG_SIZE] = {0};  // Fixed: Added buffer declaration
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
        //write_queue_to_log();
       
         // Prompt to view log after adding job
        printf("\nJob added. View log file? (1 = Yes, 0 = No): ");
        int view_log;
        scanf("%d", &view_log);
        getchar();
       
        if (view_log == 1) {
            view_log_file();
        }
        printf("\n📥 [SERVER]: Job Received: ID = %d, Filename = %s\n",new_job.jobid, new_job.filename);
       
       
        if (strcmp(new_job.content, "exit") == 0)
        {
            printf("\n[SERVER]: 'exit' job received from client. Closing connection...\n");
            break;
        }
    }
    //free_pages(client_socket);  // Clean up memory
    close(client_socket);
    free(arg);
    return NULL;
}

         


void start_server()
{
    init_memory_system();
    printf("\nSelect Scheduling Algorithm:\n");
    printf("1. First-Come-First-Serve (FCFS)\n");
    printf("2. Round Robin\n");
    printf("3. Priority\n");
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
       
        printf("New connection from %s\n", inet_ntoa(address.sin_addr));
       
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
    printf("🖨️  Server is now online and ready to receive print jobs.....\n📡  Waiting for client connections...\n\n");
   
    init_queue();
    start_server();
   
    return 0;
}


