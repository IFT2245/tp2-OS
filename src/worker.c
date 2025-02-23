#include "worker.h"
#include <stdio.h>
#include <unistd.h>

void simulate_process(process_t* p){
    if(!p) return;
    printf("[Worker] Full run => priority=%d, burst=%lu\n",
           p->priority,(unsigned long)p->burst_time);
    usleep(p->burst_time*1000U);
}

void simulate_process_partial(process_t* p, unsigned long slice_ms){
    if(!p || !slice_ms) return;
    printf("[Worker] Partial => priority=%d, slice=%lu\n",
           p->priority, slice_ms);
    usleep(slice_ms*1000U);
}
