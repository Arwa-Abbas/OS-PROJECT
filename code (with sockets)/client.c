#include "communication.h"
#include <arpa/inet.h>
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
    num_accounts++;
    return 1;
}


int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
   
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nSocket Creation Error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    char server_ip[16];
    printf("\nEnter Server IP Address to Connect: ");
    scanf("%15s", server_ip);
    getchar();

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }

    int pid = getpid();
    printf("\n[CLIENT %d] Connected to Server at %s:%d\n", pid, server_ip, PORT);

    struct message msg;
    msg.mestype = 1;
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

    while (1) {
        printf("\n1. Add a Printing Job\n");
        printf("2. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar();

        if (choice == 2) {
            printf("Exiting...\n");
            break;
        }

        printf("\n1. Submit Existing File\n");
        printf("2. Create a New file\n");
        printf("Enter your choice: ");
        scanf("%d", &msg.job_type);
        getchar();

        if (msg.job_type == 1) {
            printf("Enter filename: ");
            fgets(msg.mesfilename, sizeof(msg.mesfilename), stdin);
            msg.mesfilename[strcspn(msg.mesfilename, "\n")] = '\0';
           
            // Default priority (server may or may not use it)
            msg.priority = 5;
           
            send(sock, &msg, sizeof(msg), 0);

            char buffer[MSG_SIZE] = {0};
            read(sock, buffer, MSG_SIZE);
            printf("\nFile content:\n%s\n", buffer);
        }
        else if (msg.job_type == 2) {
            printf("Enter filename: ");
            fgets(msg.mesfilename, sizeof(msg.mesfilename), stdin);
            msg.mesfilename[strcspn(msg.mesfilename, "\n")] = '\0';

            // Always ask for priority since client doesn't know server's algorithm
            printf("Enter priority (1-10, 1=highest): ");
            scanf("%d", &msg.priority);
            getchar();

            printf("Heading: ");
            fgets(msg.mesheading, sizeof(msg.mesheading), stdin);
            msg.mesheading[strcspn(msg.mesheading, "\n")] = '\0';

            printf("Content: ");
            fgets(msg.mescontent, sizeof(msg.mescontent), stdin);
            msg.mescontent[strcspn(msg.mescontent, "\n")] = '\0';

            send(sock, &msg, sizeof(msg), 0);

            char buffer[MSG_SIZE] = {0};
            read(sock, buffer, MSG_SIZE);
            printf("\nServer response: %s\n", buffer);
        }
    }

    close(sock);
    return 0;
}
