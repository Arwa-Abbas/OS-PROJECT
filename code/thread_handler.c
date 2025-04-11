#include "communication.h"
#include <pthread.h>

#define NUM_THREADS 3

extern JobQueue *queue;
extern int semid;

/*void sem_wait(int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(semid, &op, 1);
}

void sem_signal(int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(semid, &op, 1);
}*/

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;

    while (1) {
        sem_wait(FULL);
        sem_wait(MUTEX);

        Job job = queue->jobs[queue->front];
        queue->front = (queue->front + 1) % MAX_JOBS;
        queue->count--;

        sem_signal(MUTEX);
        sem_signal(EMPTY);

        if (strcmp(job.content, "exit") == 0) {
            printf("[THREAD %d] Received exit signal. Exiting...\n", thread_id);
            break;
        }

        sleep(1); // simulate processing
        printf("[THREAD %d] Processing Job ID = %d | Content: %s\n", thread_id, job.jobid, job.content);
    }

    return NULL;
}

void start_thread_pool() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, worker_thread, &thread_ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}
