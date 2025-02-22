#include "worker.h"
#include <stdio.h>
#include <unistd.h>

/*
   Simulates the full burst of a process
   We approximate by usleep(burst_time * 1000).
*/
void simulate_process(process_t* p){
    if(!p) return;
    printf("Simulating process with burst=%lu ms, priority=%d\n",
           (unsigned long)p->burst_time, p->priority);
    usleep((useconds_t)(p->burst_time * 1000U));
}

/*
   Simulates a partial timeslice for a process
   We approximate by usleep(slice_ms * 1000).
*/
void simulate_process_partial(process_t* p, unsigned long slice_ms){
    if(!p || slice_ms == 0) return;
    printf("Simulating partial process for %lu ms, priority=%d\n",
           slice_ms, p->priority);
    usleep((useconds_t)(slice_ms * 1000U));
}
