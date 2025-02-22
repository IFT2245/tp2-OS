#include "worker.h"
#include <stdio.h>
#include <unistd.h>

void simulate_process(process_t* p){
    if(!p) return;
    printf("[Worker] Full process burst=%lu ms, priority=%d\n",
           (unsigned long)p->burst_time, p->priority);
    usleep((useconds_t)(p->burst_time * 1000U));
}

void simulate_process_partial(process_t* p, unsigned long slice_ms){
    if(!p || !slice_ms) return;
    printf("[Worker] Partial process %lu ms, priority=%d\n",
           slice_ms, p->priority);
    usleep((useconds_t)(slice_ms * 1000U));
}
