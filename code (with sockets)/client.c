#include "communication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>     //for ip address handling
#include <netdb.h>

#define TEXT_LIMIT 512

int main() 
{
    int sock=0;
    struct sockaddr_in serv_addr;
    if ((sock=socket(AF_INET,SOCK_STREAM,0)) < 0) 
    {
        printf("\n Socket Creation Error \n");
        return -1;
    }
    
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(PORT);
    
    char server_ip[16];
    printf("\nEnter Server IP Address to Connect: ");
    scanf("%15s",server_ip);
    getchar();
    
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)   //this takes ip address
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    if (connect(sock,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0) 
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    int pid=getpid();
    printf("[CLIENT %d] Connected to Server at %s:%d\n",pid,server_ip,PORT);
    printf("\n------------WELCOME [CLIENT %d]---------------\n",pid);
    
    char jobtext[TEXT_LIMIT];
    struct message msg;
    msg.mestype=1;
    int choice;

    while (1) 
    {
        printf("\nDo you want to:\n");
        printf("1. Add a Printing Job\n");
        printf("2. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar(); 
        
        if (choice==2) 
        {
            printf("Exiting...\n");
            break;
        }
        else if (choice==1) 
        {
            printf("\n------------------------------------------------------------\n");
            printf("Enter 'exit' if you want to discontinue");
            printf("\n------------------------------------------------------------\n\n");       
            char jobtext[TEXT_LIMIT] = "";  
            char filename[TEXT_LIMIT], heading[TEXT_LIMIT], content[TEXT_LIMIT];
            
            printf("[CLIENT %d] Filename: ", pid);
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';
            if (strcmp(filename, "exit") == 0)
            break;
           
            printf("\n---------Now Enter Content of File---------\n");
            printf("\nHeading: ");
            fgets(heading, sizeof(heading), stdin);
            heading[strcspn(heading, "\n")] = '\0';
            if (strcmp(heading, "exit") == 0)
            break;

            printf("Content: ");
            fgets(content, sizeof(content), stdin);
            content[strcspn(content, "\n")] = '\0';
           if (strcmp(content, "exit") == 0)
           break;

           strncpy(msg.mesfilename, filename, TEXT_LIMIT);
           strncpy(msg.mesheading, heading, TEXT_LIMIT);
           strncpy(msg.mescontent, content, TEXT_LIMIT);
           send(sock, &msg, sizeof(msg), 0);
           printf("[CLIENT %d] Sent File: %s | Heading: %s | Content: %s\n", pid, msg.mesfilename,msg.mesheading,msg.mescontent);

           char buffer[MSG_SIZE] = {0};
           read(sock, buffer, MSG_SIZE);
           printf("[CLIENT %d] Server Response: %s\n", pid, buffer);
         }
        else 
        {
            printf("Invalid option. Please try again.\n");
        }
    }   
    close(sock);
    return 0;
}
