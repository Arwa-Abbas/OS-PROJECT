#include "communication.h"
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include<stdbool.h>

JobQueue *queue = NULL;
int semid;

void sem_wait(int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(semid, &op, 1);
}

void sem_signal(int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(semid, &op, 1);
}

void init_ipc() {
	
int shmid = shmget(SHM_KEY, sizeof(JobQueue), IPC_CREAT | IPC_EXCL | 0666);
bool newly_created = true;

if (shmid == -1) {
    // Shared memory already exists, get it instead
    shmid = shmget(SHM_KEY, sizeof(JobQueue), IPC_CREAT | 0666);
    newly_created = false;
}

queue = (JobQueue*)shmat(shmid, NULL, 0);

// Only reset if it's newly created
if (newly_created) {
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
}


    semid = semget(SEM_KEY, NUM_SEMAPHORES, IPC_CREAT | 0666);
    semctl(semid, MUTEX, SETVAL, 1);
    semctl(semid, FULL, SETVAL, 0);
    semctl(semid, EMPTY, SETVAL, MAX_JOBS);
}

void process_jobs() {
    int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    struct message msg;
    int job_id = 1;

    while (1) {
        msgrcv(msgid, &msg, sizeof(msg.mestext), 1, 0);

        sem_wait(EMPTY);
        sem_wait(MUTEX);

        Job new_job;
        new_job.jobid = job_id++;
        strcpy(new_job.content, msg.mestext);

        queue->jobs[queue->rear] = new_job;
        queue->rear = (queue->rear + 1) % MAX_JOBS;
        queue->count++;

        printf("[SERVER]:- Job Received: ID = %d, CONTENT = %s\n", new_job.jobid, new_job.content);

        sem_signal(MUTEX);
        sem_signal(FULL);

        if (strcmp(msg.mestext, "exit") == 0)
            break;
    }
}

void start_thread_pool(); // declared from thread_handler.c

int main() {
    printf("---------------------MultiUser Print Server--------------------\n");

    init_ipc();

    if (fork() == 0) {
        process_jobs(); // child process adds jobs to queue
    } else {
        start_thread_pool(); // parent process processes jobs
        wait(NULL); // wait for child to finish
    }

    return 0;
}


