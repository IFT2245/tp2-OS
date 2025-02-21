#include "process.h"
#include <string.h>

void init_process(process_t* p, uint64_t burst, int prio, uint64_t arrival){
    if(!p) return;
    memset(p,0,sizeof(*p));
    p->burst_time=burst;
    p->priority=prio;
    p->vruntime=0;
    p->arrival_time=arrival;
    p->remaining_time=burst;
    p->last_exec=0;
}
