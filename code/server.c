#include "communication.h"
#include <sys/wait.h>
#include "thread_handler.h"

int semid, shmid, msgid;
JobQueue *queue;

void sem_wait(int sem_num)       //wait for resource decreasing value of sem by 1
{
    struct sembuf sb={sem_num,-1,0};   
    semop(semid,&sb,1);
}

void sem_signal(int sem_num)     //release a resource increasing value of sem by 1
{
    struct sembuf sb={sem_num, 1, 0};
    semop(semid, &sb, 1);
}

void init_ipc() 
{
    // shared memory (create/access shared memory and attach to process)
    shmid = shmget(SHM_KEY, sizeof(JobQueue), IPC_CREAT | 0666);
    queue = (JobQueue *)shmat(shmid, NULL, 0);
    queue->front = queue->rear = queue->count = 0;

    // semaphores
    semid=semget(SEM_KEY, NUM_SEMAPHORES, IPC_CREAT | 0666);
    semctl(semid, MUTEX, SETVAL, 1);          //mutual exclusion
    semctl(semid, FULL, SETVAL, 0);           //0 slots
    semctl(semid, EMPTY, SETVAL, MAX_JOBS);   //initially slots are empty

    // message queue
    msgid=msgget(MSG_KEY, IPC_CREAT | 0666);
}

void process_jobs()      //adding job to queue
{
    struct message msg;
    int job_id = 1;
    
    while (1) 
    {
        msgrcv(msgid,&msg,sizeof(msg.mestext), 1, 0);

        sem_wait(EMPTY);
        sem_wait(MUTEX);

        Job newjob;
        newjob.jobid=job_id++;
        strncpy(newjob.content,msg.mestext,MSG_SIZE);
        queue->jobs[queue->rear]=newjob;
        queue->rear = (queue->rear + 1) % MAX_JOBS;
        queue->count++;

        printf("[SERVER]:- Job Received: ID = %d, CONTENT = %s\n", newjob.jobid, newjob.content);

        sem_signal(MUTEX);
        sem_signal(FULL);
        
        if (strcmp(newjob.content,"exit")==0) 
        {
        printf("\nServer Exiting...\n");
        break;
        }
    }
}

int main() 
{
    printf("\n---------------------------------------------------------------\n");
    printf("---------------------MultiUser Print Server--------------------\n");
    printf("---------------------------------------------------------------\n\n");
    init_ipc();
    start_thread_pool(queue, semid);
    stop_thread_pool();
    return 0;
}
