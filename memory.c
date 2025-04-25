#include "communication.h"
#include <stdlib.h>

PhysicalMemory phys_mem;
ProcessMemory* processes[MAX_CLIENTS];
int process_count = 0;

void init_memory_system() {
    // Initialize physical memory
    phys_mem.free_count = NUM_FRAMES;
    for (int i = 0; i < NUM_FRAMES; i++) {
        phys_mem.free_frames[i] = 1; // 1 means free
    }
}

ProcessMemory* create_process_memory(pid_t pid) {
    ProcessMemory* proc = malloc(sizeof(ProcessMemory));
    proc->pid = pid;
    proc->page_table = calloc(1024, sizeof(PageTableEntry)); // Assume 1024 max pages
    proc->page_count = 0;
    processes[process_count++] = proc;
    return proc;
}

int allocate_page(pid_t pid) {
    // Find the process
    ProcessMemory* proc = NULL;
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == pid) {
            proc = processes[i];
            break;
        }
    }
    if (!proc) return -1;

    // Find a free frame
    if (phys_mem.free_count == 0) {
        // Simple page replacement - just pick first frame
        // In real OS this would be more sophisticated (LRU, etc.)
        int frame_to_replace = 0;
       
        // Mark all processes that were using this frame as not present
        for (int i = 0; i < process_count; i++) {
            for (int j = 0; j < processes[i]->page_count; j++) {
                if (processes[i]->page_table[j].present &&
                    processes[i]->page_table[j].frame == frame_to_replace) {
                    processes[i]->page_table[j].present = 0;
                    if (processes[i]->page_table[j].modified) {
                        // Would write to disk here in real system
                        processes[i]->page_table[j].modified = 0;
                    }
                }
            }
        }
       
        phys_mem.free_frames[frame_to_replace] = 1;
        phys_mem.free_count++;
    }

    // Allocate the frame
    int frame_num = -1;
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (phys_mem.free_frames[i]) {
            frame_num = i;
            break;
        }
    }
    //if (frame_num == -1) return -1;
      if (frame_num == -1) {
      phys_mem.free_frames[frame_num] = 0;  // Mark frame as used
        phys_mem.free_count--;
        printf("DEBUG: Allocated frame %d (now %d/%d used)\n",
              frame_num, NUM_FRAMES - phys_mem.free_count, NUM_FRAMES);
     }
    // Set up page table entry
    int page_num = proc->page_count++;
    proc->page_table[page_num].present = 1;
    proc->page_table[page_num].frame = frame_num;
    proc->page_table[page_num].modified = 0;
   
    phys_mem.free_frames[frame_num] = 0;
    phys_mem.free_count--;
   
    return page_num;
}
int count_frames_for_job(int client_socket) {
    int count = 0;
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == client_socket) {
            for (int j = 0; j < processes[i]->page_count; j++) {
                if (processes[i]->page_table[j].present) {
                    count++;
                }
            }
        }
    }
    return count;
}

char* translate_address(pid_t pid, int virtual_address) {
    int page_num = virtual_address / PAGE_SIZE;
    int offset = virtual_address % PAGE_SIZE;
   
    // Find the process
    ProcessMemory* proc = NULL;
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == pid) {
            proc = processes[i];
            break;
        }
    }
    if (!proc || page_num >= proc->page_count || !proc->page_table[page_num].present) {
        return NULL; // Page fault
    }
   
    int frame_num = proc->page_table[page_num].frame;
    return &phys_mem.frames[frame_num][offset];
}

void free_pages(pid_t pid) {
    // Find and free all pages for a process
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == pid) {
            for (int j = 0; j < processes[i]->page_count; j++) {
                if (processes[i]->page_table[j].present) {
                    phys_mem.free_frames[processes[i]->page_table[j].frame] = 1;
                    phys_mem.free_count++;
                }
            }
            free(processes[i]->page_table);
            free(processes[i]);
            processes[i] = NULL;
            break;
        }
    }
}



/*int job_in_memory(int client_socket, int job_id) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == client_socket) {
            for (int j = 0; j < processes[i]->page_count; j++) {
                if (processes[i]->page_table[j].present) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int get_frame_number(int client_socket, int job_id) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i]->pid == client_socket) {
            for (int j = 0; j < processes[i]->page_count; j++) {
                if (processes[i]->page_table[j].present) {
                    return processes[i]->page_table[j].frame;
                }
            }
        }
    }
    return -1;
}*/
