Welcome to the Multi-User Print Server Project!
This system simulates how multiple clients send print jobs to a central server. The server handles scheduling, memory management, resource allocation, and job processing in a concurrent environment.

## Overview
Concurrent Clients: Clients register/login and submit print jobs.
Job Types and Job Scheduling: Upload existing files or create new files and choose job scheduling method on server.
Memory Management: 8 virtual frames, 4 pages per job, FIFO replacement.
Resource Management: Reader-writer locks for controlled access.
Logging: Clients can view job logs after submissions.
Resource Heirachy Dealock: dining philospher critical problem implemented.

## Tech Stack
- **Language**: C
- **Concurrency**: pthreads
- **Synchronization**: Mutexes, semaphores, condition variables
- **Memory**: Simulated virtual memory system

## Compilation:
gcc -o server server.c memory.c -lpthread
gcc -o client client.c -lpthread
or
gcc server.c memory.c -o server
gcc client.c -o client

# Run (in separate terminals)
./server 
./client (can run multiple)
