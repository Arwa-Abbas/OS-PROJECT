#include "communication.h"
#define TEXT_LIMIT 240

int main() 
{
    int msgid=msgget(MSG_KEY, 0666);
    if (msgid==-1)
     {
        perror("ERROR: msgget failed");
        return 1;
    }

    struct message msg;
    msg.mestype=1;
    int pid=getpid();

    char jobtext[TEXT_LIMIT];

    printf("[Client %d] Enter Jobs ('exit' to quit):\n", pid);

    //job input from clients
    while (1) 
    {
        printf("JOB: ");
        fgets(jobtext,TEXT_LIMIT,stdin);
        jobtext[strcspn(jobtext,"\n")]='\0';
       if (strcmp(jobtext,"exit") == 0)
            break;
        snprintf(msg.mestext,MSG_SIZE,"%d %s",pid,jobtext);

        if (msgsnd(msgid, &msg,sizeof(msg.mestext),0)==-1) 
            perror("ERROR: msgsnd failed");
        else
            printf("[CLIENT %d] Sent:- %s\n",pid,jobtext);

    }
    return 0;
}
