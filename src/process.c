#include "process.h"
#include "../lib/log.h"

void init_process(process_t* p, unsigned long burst, int prio, unsigned long arrival, double weight){
    if(!p){
        log_error("init_process => Null pointer for process_t");
        return;
    }
    if(burst == 0){
        log_warn("init_process => burst=0 => completes instantly");
    }
    if(prio < 0){
        log_warn("init_process => negative priority => continuing");
    }
    if(weight <= 0.0){
        log_warn("init_process => nonpositive weight => forcing weight=1.0");
        weight = 1.0;
    }

    p->id             = 0;
    p->burst_time     = burst;
    p->priority       = prio;
    p->arrival_time   = arrival;
    p->remaining_time = burst;
    p->first_response = 0;
    p->end_time       = 0;
    p->responded      = false;
    p->weight         = weight;
    p->hpc_affinity   = -1;
    p->mlfq_level     = 0;
    p->was_preempted  = false;
}
