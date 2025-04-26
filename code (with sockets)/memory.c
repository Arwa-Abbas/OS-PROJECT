#include "communication.h"
#include <stdlib.h>

void load_job_pages_into_memory(Job job, int thread_id)
{
    JobMemory *job_mem = job_memories[job.jobid % MAX_JOBS];
    if (!job_mem)
    {
        printf("[THREAD %d] ERROR: No memory allocated for Job %d!\n", thread_id, job.jobid);
        return;
    }

    for (int i = 0; i < PAGES_PER_JOB; i++)
    {
        if (job_mem->pages[i].frame == -1)
        {
            for (int j = 0; j < NUM_FRAMES; j++)
            {
                int is_frame_free = 1;
                for (int k = 0; k < MAX_JOBS; k++)
                {
                    if (job_memories[k])
                    {
                        for (int p = 0; p < PAGES_PER_JOB; p++)
                        {
                            if (job_memories[k]->pages[p].frame == j)
                            {
                                is_frame_free = 0;
                                break;
                            }
                        }
                    }
                }

                if (is_frame_free)
                {
                    job_mem->pages[i].frame = j;
                    used_frames++;
                    printf("[THREAD %d] Loaded Job %d Page %d → Frame %d\n",
                           thread_id, job.jobid, i, j);
                    break;
                }
            }
        }
    }
}
