
#include "communication.h"
#include <stdlib.h>


void load_job_pages_into_memory(Job job,int thread_id)
{
    JobMemory *job_mem=job_memories[job.jobid % MAX_JOBS];
    if (!job_mem)
    {
        printf("[THREAD %d] ERROR: No Memory Allocated for Job %d!\n", thread_id, job.jobid);
        return;
    }

    for (int i=0;i<PAGES_PER_JOB;i++)
    {
        if (job_mem->pages[i].frame==-1)
        {
            if (used_frames<NUM_FRAMES)
            {
                //Free frame is available
                for (int j = 0; j < NUM_FRAMES; j++)
                {
                    int is_frame_free=1;
                    for (int k = 0; k < MAX_JOBS; k++)
                    {
                        if (job_memories[k])
                        {
                            for (int p = 0; p < PAGES_PER_JOB; p++)
                            {
                                if (job_memories[k]->pages[p].frame == j)
                                {
                                    is_frame_free=0;
                                    break;
                                }
                            }
                        }
                    }

                    if (is_frame_free)
                    {
                        job_mem->pages[i].frame=j;
                        frame_queue[frame_queue_rear % NUM_FRAMES]=j;
                        frame_queue_rear++;
                        used_frames++;
                        printf("\n[THREAD %d] Loaded Job %d Page %d → Frame %d",
                               thread_id, job.jobid, i, j);
                        break;
                    }
                }
            }
            else
            {
                // NOw No free frame ( FIFO replacement algorithm)
                int frame_to_replace=frame_queue[frame_queue_front % NUM_FRAMES];
                frame_queue_front++;

                //find and remove the page currently which uses this frame
                for (int k=0;k<MAX_JOBS;k++)
                {
                    if (job_memories[k])
                    {
                        for (int p=0;p<PAGES_PER_JOB;p++)
                        {
                            if (job_memories[k]->pages[p].frame==frame_to_replace)
                            {
                                job_memories[k]->pages[p].frame = -1;
                                break;
                            }
                        }
                    }
                }

                // replaced frame to the new page
                job_mem->pages[i].frame = frame_to_replace;
                frame_queue[frame_queue_rear % NUM_FRAMES] = frame_to_replace;
                frame_queue_rear++;

                printf("\n[THREAD %d] (FIFO REPLACEMENT ALGORITHM) Loaded Job %d Page %d → Frame %d",
                       thread_id, job.jobid, i, frame_to_replace);
            }
        }
    }
}
