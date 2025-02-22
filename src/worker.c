#include "worker.h"
#include <stdio.h>
#include <unistd.h>

void simulate_process(process_t* p){
    if(!p)return;
    printf("[Worker] Full run => priority=%d, burst=%lu\n",p->priority,(unsigned long)p->burst_time);
    usleep(p->burst_time*1000U);
}

void simulate_process_partial(process_t* p,unsigned long slice){
    if(!p||!slice)return;
    printf("[Worker] Partial => priority=%d, slice=%lu\n",p->priority,slice);
    usleep(slice*1000U);
}
