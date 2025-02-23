#include "worker.h"
#include <stdio.h>
#include <unistd.h>

/* The original full-run is rarely used, but let's keep it. */
void simulate_process(process_t* p){
    if(!p) return;
    printf("[Worker] Full run => priority=%d, burst=%lu ms\n",
           p->priority,(unsigned long)p->burst_time);
    /* We do real-time sleep just for user experience. */
    usleep(p->burst_time * 1000U);
}

/*
  simulate_process_partial(p, slice):
   - Sleep for 'slice' ms in real time, purely for demonstration.
*/
void simulate_process_partial(process_t* p, unsigned long slice_ms){
    if(!p || !slice_ms) return;
    printf("[Worker] Partial => priority=%d, slice=%lu ms\n",
           p->priority, slice_ms);
    usleep(slice_ms * 1000U);
}
