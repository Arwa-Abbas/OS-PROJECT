#include "communication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>     //for ip address handling
#include <netdb.h>

#define TEXT_LIMIT 512
#define MAX_ACCOUNTS 10

struct Account
{
char username[TEXT_LIMIT];
char password[TEXT_LIMIT];
};

struct Account accounts[MAX_ACCOUNTS];
int num_accounts=0;

// Load accounts from file
void load_accounts()
{
    FILE *file = fopen("accounts.txt", "r");
    if (file == NULL) return;
    while (fscanf(file, "%s %s", accounts[num_accounts].username, accounts[num_accounts].password) == 2)
    {
        num_accounts++;
        if (num_accounts >= MAX_ACCOUNTS) break;
    }
    fclose(file);
}

// Save a new account to file
void save_account(const char *username, const char *password)
{
    FILE *file = fopen("accounts.txt", "a");
    if (file != NULL)
    {
        fprintf(file, "%s %s\n", username, password);
        fclose(file);
    }
}

int login(char *username, char *password)
{
    for (int i=0;i<num_accounts;i++)
    {
        if (strcmp(accounts[i].username, username) == 0 &&
            strcmp(accounts[i].password, password) == 0)
            {
            return 1;
        }
    }
    return 0;
}


int create_account(char *username, char *password)
{
    if (num_accounts >= MAX_ACCOUNTS)
    {
        printf("Account limit reached. Cannot create a new account.\n");
        return 0;
    }

    // Check if the username already exists
    for (int i = 0; i < num_accounts; i++)
    {
        if (strcmp(accounts[i].username, username) == 0)
        {
            printf("Username already exists.\n");
            return 0;
        }
    }
    strcpy(accounts[num_accounts].username, username);
    strcpy(accounts[num_accounts].password, password);
    save_account(username, password);
    num_accounts++;
    return 1;
}


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
   
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
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
   
    load_accounts();
    char jobtext[TEXT_LIMIT];
    struct message msg;
    msg.mestype=1;
    int choice;
   
    char username[TEXT_LIMIT],password[TEXT_LIMIT];
    int logged_in = 0;
   
     while (!logged_in)
     {
        printf("\n1. Login\n");
        printf("2. Create Account\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar();
       
        if (choice == 1)
        {
            printf("\nEnter Username: ");
            fgets(username, sizeof(username), stdin);
            username[strcspn(username, "\n")] = '\0';

            printf("Enter Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = '\0';

            if (login(username, password))
            {
                printf("Login successful.\n");
                logged_in = 1;
            }
            else
            {
                printf("Invalid username or password. Please try again.\n");
            }
        }
        else if (choice == 2)
        {
            printf("\nEnter a new Username: ");
            fgets(username, sizeof(username), stdin);
            username[strcspn(username, "\n")] = '\0';

            printf("Enter a new Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = '\0';

            if (create_account(username, password))
            {
                printf("Account created successfully\n");
            }
            else
            {
                printf("Account creation failed.\n");
            }
        }
        else
        {
            printf("Invalid option. Please try again.\n");
        }
    }
   
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
             int job_type;
             printf("\n-----------Choose Job Type--------------\n\n");
             printf("1. Submit Existing File (server will read the file)\n");
             printf("2. Create a New file (you provide heading and content)\n");
             printf("Enter your choice: ");
             scanf("%d", &job_type);
             getchar();
             
             if (job_type == 1) {
            printf("\n------------------------------------------------------------\n");
            char filename[TEXT_LIMIT];
            printf("[CLIENT %d] filename: ", pid);
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = '\0';
           
            if (strcmp(filename, "exit") == 0) {
                break;
            }
           
            strncpy(msg.mesfilename, filename, TEXT_LIMIT);
            msg.job_type = 1;
           
    // Read file content
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        printf("[CLIENT %d] Could not open file: %s\n", pid, filename);
        continue;  // Skip to next iteration
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file content
    char *file_content = (char*)malloc(file_size + 1);
    size_t bytes_read = fread(file_content, 1, file_size, file);
    file_content[bytes_read] = '\0';
    fclose(file);

              int priority;
              printf("Enter Priority (1-5, 1=highest): ");
              scanf("%d", &priority);
              getchar();
           
            msg.priority=priority;
            strncpy(msg.mesfilename, filename, TEXT_LIMIT);
            strncpy(msg.mescontent,file_content, TEXT_LIMIT);
   
            send(sock, &msg, sizeof(msg), 0);
            printf("[CLIENT %d] Sent request to print existing file: %s\n", pid, filename);
             
            char buffer[MSG_SIZE] = {0};
            read(sock, buffer, MSG_SIZE);
            printf("[CLIENT %d] Server Response: %s\n", pid, buffer);
        }
           
            else if (job_type==2)
            {
                printf("\n------------------------------------------------------------\n");
                printf("Enter 'exit' if you want to discontinue");
                printf("\n------------------------------------------------------------\n\n");      
                char filename[TEXT_LIMIT], heading[TEXT_LIMIT], content[TEXT_LIMIT];
                int priority;
               
                printf("[CLIENT %d] Filename: ", pid);
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = '\0';
               
                if (strcmp(filename, "exit") == 0)
                    break;
                   
                 printf("Enter Priority (1-5, 1=highest): ");
                 scanf("%d", &priority);
                 getchar();
               
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

                msg.job_type = 2;  // Explicitly set job type to 2
                strncpy(msg.mesfilename, filename, TEXT_LIMIT);
                strncpy(msg.mesheading, heading, TEXT_LIMIT);
                strncpy(msg.mescontent, content, TEXT_LIMIT);
                msg.priority=priority;
                send(sock, &msg, sizeof(msg), 0);
                printf("[CLIENT %d] Sent File: %s | Heading: %s | Content: %s\n", pid, msg.mesfilename,msg.mesheading,msg.mescontent);

                char buffer[MSG_SIZE] = {0};
                read(sock, buffer, MSG_SIZE);
                printf("[CLIENT %d] Server Response: %s\n", pid, buffer);
            }
        }
        else
        {
            printf("Invalid option. Please try again.\n");
        }
    }  
    close(sock);
    //free_pages(getpid());
    return 0;
}
