#include "communication.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#define NUM_THREADS 10
JobQueue *queue = NULL;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

void init_queue() {
    queue = (JobQueue*)malloc(sizeof(JobQueue));
    queue->count = 0;
    queue->current_algorithm = FCFS;
    queue->rr_counter = 0;
}

int add_job_to_queue(Job new_job) {
    pthread_mutex_lock(&queue_mutex);
    while (queue->count == MAX_JOBS) {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
    }

    int pos = queue->count; // Default to append
   
    if (queue->current_algorithm == PRIORITY) {
        for (pos = 0; pos < queue->count; pos++) {
            if (new_job.priority < queue->jobs[pos].priority) {
                break;
            }
        }
    }

    for (int i = queue->count; i > pos; i--) {
        queue->jobs[i] = queue->jobs[i-1];
    }

    queue->jobs[pos] = new_job;
    queue->count++;

    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
    return 1;
}

int remove_job_from_queue(Job *job) {
    pthread_mutex_lock(&queue_mutex);
    while (queue->count == 0) {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }

    if (queue->current_algorithm == ROUND_ROBIN) {
        *job = queue->jobs[queue->rr_counter % queue->count];
        for (int i = queue->rr_counter % queue->count; i < queue->count-1; i++) {
            queue->jobs[i] = queue->jobs[i+1];
        }
        queue->rr_counter = (queue->rr_counter + 1) % NUM_THREADS;
    } else {
        *job = queue->jobs[0];
        for (int i = 0; i < queue->count-1; i++) {
            queue->jobs[i] = queue->jobs[i+1];
        }
    }
   
    queue->count--;

    pthread_cond_signal(&queue_not_full);
    pthread_mutex_unlock(&queue_mutex);
    return 1;
}

void set_scheduling_algorithm(int algorithm) {
    pthread_mutex_lock(&queue_mutex);
    queue->current_algorithm = algorithm;
    queue->rr_counter = 0;
    pthread_mutex_unlock(&queue_mutex);
   
    printf("\nScheduling algorithm set to: ");
    switch(algorithm) {
        case FCFS: printf("First-Come-First-Serve\n"); break;
        case ROUND_ROBIN: printf("Round Robin\n"); break;
        case PRIORITY: printf("Priority\n"); break;
        default: printf("Unknown\n");
    }
}

void log_job_to_file(Job job) {
    FILE* log_fp = fopen("queue_log.txt", "a");
    if (log_fp == NULL) return;

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    const char* algo_str;
    switch(queue->current_algorithm) {
        case FCFS: algo_str = "FCFS"; break;
        case ROUND_ROBIN: algo_str = "Round Robin"; break;
        case PRIORITY: algo_str = "Priority"; break;
        default: algo_str = "Unknown";
    }

    fprintf(log_fp, "Client %d | Job ID: %d | Filename: %s | Priority: %d | Arrival: %s | Algorithm: %s\n",
            job.client_socket, job.jobid, job.filename, job.priority, time_str, algo_str);

    fclose(log_fp);
}

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    while (1) {
        Job job;
        remove_job_from_queue(&job);

        printf("\n[THREAD %d] Processing Job ID = %d | Filename: %s", thread_id, job.jobid, job.filename);
        if (queue->current_algorithm == PRIORITY) {
            printf(" | Priority: %d", job.priority);
        }
        printf("\n");
       
        if (job.job_type == 1) {
            printf("[THREAD %d] Printing existing file content:\n%s\n", thread_id, job.content);
        } else if (job.job_type == 2) {
            printf("[THREAD %d] Creating new file: %s\n", thread_id, job.filename);
            FILE *fp = fopen(job.filename, "w");
            if (fp != NULL) {
                fprintf(fp, "%s\n%s\n", job.heading, job.content);
                fclose(fp);
            }
        }

        char ack_msg[MSG_SIZE];
        snprintf(ack_msg, MSG_SIZE, "Job %d Completed by Thread %d", job.jobid, thread_id);
        send(job.client_socket, ack_msg, strlen(ack_msg), 0);
       
        sleep(1);
    }
    return NULL;
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    struct message msg;
    int job_id = 1;

    while (1) {
        int bytes_received = recv(client_socket, &msg, sizeof(msg), 0);
        if (bytes_received <= 0) {
            printf("Client disconnected\n");
            break;
        }

        Job new_job;
        new_job.jobid = job_id++;
        new_job.client_socket = client_socket;
        new_job.job_type = msg.job_type;
        new_job.priority = msg.priority;
        new_job.arrival_time = time(NULL);
       
        strcpy(new_job.filename, msg.mesfilename);
       
        if (msg.job_type == 1) {
            FILE* fp = fopen(new_job.filename, "r");
            if (fp == NULL) {
                send(client_socket, "Error: File not found", 21, 0);
                continue;
            }
            char buffer[MSG_SIZE] = {0};
            fread(buffer, 1, MSG_SIZE - 1, fp);
            fclose(fp);
            send(client_socket, buffer, strlen(buffer), 0);
            strcpy(new_job.content, buffer);
        } else if (msg.job_type == 2) {
            strcpy(new_job.heading, msg.mesheading);
            strcpy(new_job.content, msg.mescontent);
        }
       
        add_job_to_queue(new_job);
        log_job_to_file(new_job);
        printf("\n📥 [SERVER] Received Job ID %d: %s", new_job.jobid, new_job.filename);
        if (queue->current_algorithm == PRIORITY) {
            printf(" (Priority: %d)", new_job.priority);
        }
    }
   
    close(client_socket);
    free(arg);
    return NULL;
}

void start_server() {
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

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("\n🖨️  Print Server started on port %d\n", PORT);
    printf("📡  Waiting for client connections...\n\n");

    pthread_t workers[NUM_THREADS];
    int worker_ids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        worker_ids[i] = i + 1;
        pthread_create(&workers[i], NULL, worker_thread, &worker_ids[i]);
    }

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        printf("\nNew client connected from %s\n", inet_ntoa(address.sin_addr));

        pthread_t thread_id;
        int *client_socket = malloc(sizeof(int));
        *client_socket = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_socket) < 0) {
            perror("could not create thread");
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);
}

int main() {
    printf("\n\t\t\t\tMULTI-USER PRINT SERVER\n");
    init_queue();
    start_server();
    return 0;
}
