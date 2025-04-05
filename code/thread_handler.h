
#ifndef THREAD_HANDLER_H
#define THREAD_HANDLER_H

#include "communication.h"

void start_thread_pool(JobQueue *queue,int semid);
void stop_thread_pool();

#endif
