This project proposes the design and implementation of a Multi-User Print Server in C, which simulates the core functionalities of a real-world print server. The project will focus on concepts 
such as concurrency, inter-process communication, process/thread scheduling, synchronization, deadlock prevention, and virtual memory management which are key areas in operating system design.

IMPLEMENTED
-> Client-Server Communication using Message Queues
-> Shared Memory to hold the bounded job queue
-> Semaphores
-> Safe concurrent access to share resources

PROGRESS
-> as client server is one thread start working on multithreading and threadpool in thread_handler 
-> Performed multithreading, making changes in server, client, communication. The core functionality for multithreading added in thread_handler.c file. 
