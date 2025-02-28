#include "scheduler.h"
#include "../lib/log.h"
#include <stdio.h>
#include <stdlib.h>

/* EXPLANATION:
   If HPC BFS arrives at t=5, and BFS quantum=4 => 4ms slices, each slice with
   usleep(ms * 3000) might exceed the 5s real-time limit. So we reduce to 1000. */

void do_cpu_work(unsigned long ms, int core_id, int proc_id){
    fprintf(stderr, "\033[36m[CORE %d] Running proc P%d for %lu ms...\033[0m\n",
            core_id, proc_id, ms);

    /* FIX: Lower slowdown factor from *3000 to *1000. */
    usleep((useconds_t)(ms * 1000U));
}

unsigned long get_quantum(scheduler_alg_t alg, const process_t* p){
    if(!p) return 0;
    switch(alg){
    case ALG_RR:
        return 2;
    case ALG_BFS:
        return 4; /* BFS => bigger chunk (4ms) */
    case ALG_WFQ:
        return 3;
    case ALG_MLFQ:
        /* Example MLFQ: 2 + 2 per level. */
        return (2 + p->mlfq_level*2);
    case ALG_PRIO_PREEMPT:
        return 2;
    case ALG_FIFO:
    case ALG_SJF:
    case ALG_PRIORITY:
    case ALG_HPC:
    case ALG_NONE:
    default:
        return 2;
    }
}

#include "container.h" // for container_t definition

void record_timeline(container_t* c,
                     int core_id,
                     int proc_id,
                     unsigned long start_ms,
                     unsigned long slice,
                     bool preempted_flag)
{
    /* Standard approach: reallocate timeline array if needed. */
    pthread_mutex_lock(&c->timeline_lock);

    if(c->timeline_count+1 >= c->timeline_cap){
        int new_cap = c->timeline_cap + 64;
        void* tmp = realloc(c->timeline, new_cap * sizeof(*c->timeline));
        if(!tmp){
            log_error("record_timeline => out of memory!");
            pthread_mutex_unlock(&c->timeline_lock);
            return;
        }
        c->timeline = tmp;
        c->timeline_cap = new_cap;
    }

    c->timeline[c->timeline_count].core_id         = core_id;
    c->timeline[c->timeline_count].proc_id         = proc_id;
    c->timeline[c->timeline_count].start_ms        = start_ms;
    c->timeline[c->timeline_count].length_ms       = slice;
    c->timeline[c->timeline_count].preempted_slice = preempted_flag;
    c->timeline_count++;

    pthread_mutex_unlock(&c->timeline_lock);
}
