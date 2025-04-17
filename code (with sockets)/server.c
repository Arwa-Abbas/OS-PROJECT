#include "communication.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>

#define NUM_THREADS 4           // (will chnage this as here thread does only job)

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
}

int add_job_to_queue(Job new_job) 
{
    pthread_mutex_lock(&queue_mutex);   
    while (queue->count == MAX_JOBS) 
    {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
    }
    
    queue->jobs[queue->rear] = new_job;
    queue->rear = (queue->rear + 1) % MAX_JOBS;
    queue->count++;
    
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
    
    return 1;
}

int remove_job_from_queue(Job *job) 
{
    pthread_mutex_lock(&queue_mutex); 
    while (queue->count == 0) 
    {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }
    
    *job = queue->jobs[queue->front];
    queue->front = (queue->front + 1) % MAX_JOBS;
    queue->count--;
    
    pthread_cond_signal(&queue_not_full);
    pthread_mutex_unlock(&queue_mutex);
    
    return 1;
}

void* worker_thread(void* arg) 
{
    int thread_id = *(int*)arg;
    while (1) 
    {
        Job job;
        remove_job_from_queue(&job);

        printf("[THREAD %d] Processing Job ID = %d | Filename: %s | Heading: %s | Content: %s\n\n", 
         thread_id, job.jobid, job.filename, job.heading, job.content);
        
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
            printf("[THREAD %d] Error creating file %s.\n", thread_id, job.filename);
        }
        
        // Send acknowledgment back to client
        char ack_msg[MSG_SIZE];
        snprintf(ack_msg, MSG_SIZE, "Job %d Completed by Thread %d", job.jobid, thread_id);
        send(job.client_socket, ack_msg, strlen(ack_msg), 0);
        
        sleep(1); // Simulate processing time
        
        if (strcmp(job.content, "exit") == 0) 
        {
            printf("[THREAD %d] Received exit signal. Exiting...\n", thread_id);
            break;
        }
    }
    return NULL;
}

void* handle_client(void* arg) 
{
    int client_socket=*(int*)arg;
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
        
        strcpy(new_job.filename, msg.mesfilename);
        strcpy(new_job.heading, msg.mesheading);
        strcpy(new_job.content, msg.mescontent);
        
        // Add job to queue
        add_job_to_queue(new_job);
        printf("📥 [SERVER]: Job Received: ID = %d, Filename = %s, Heading = %s, Content = %s\n", 
                new_job.jobid, new_job.filename, new_job.heading, new_job.content);
        
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
        
        // Detach the thread so we don't have to join it
        pthread_detach(thread_id);
    }
    
    // Cleanup (though we may never reach here)
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
