#include "scheduler.h"
#include "../lib/log.h"
#include "../lib/scheduler_alg.h"
#include "../lib/library.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

unsigned long get_quantum(scheduler_alg_t alg, const process_t* p){
    if(!p) return 0;
    switch(alg){
    case ALG_RR:           return 2;
    case ALG_BFS:          return 4;
    case ALG_WFQ:          return 3;
    case ALG_MLFQ:         return (2 + p->mlfq_level * 2);
    case ALG_PRIO_PREEMPT: return 2;
    default:
        return 2;
    }
}

void record_timeline(container_t* c, int core_id, int proc_id,
                     unsigned long start_ms, unsigned long slice, bool preempted)
{
    pthread_mutex_lock(&c->timeline_lock);
    if(c->timeline_count + 1 >= c->timeline_cap){
        int new_cap = c->timeline_cap + 64;
        void* tmp = realloc(c->timeline, new_cap * sizeof(*c->timeline));
        if(!tmp){
            log_error("record_timeline => OOM");
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
    c->timeline[c->timeline_count].preempted_slice = preempted;
    c->timeline_count++;
    pthread_mutex_unlock(&c->timeline_lock);
}

/**
 * @brief CPU work with “maximum immediacy” preemption. We
 *        - unblock SIGALRM so we can be interrupted,
 *        - do the requested ms of CPU time,
 *        - block SIGALRM again before returning.
 */
void do_cpu_work(unsigned long ms, int core_id, int proc_id)
{
    if(ms == 0) return;

    unblock_preempt_signal();  // allow preemption now

    // A simple approach: loop ms times, each iteration => 1ms real-time sleep
    for (unsigned long i=0; i<ms; i++){
        usleep(1000);
    }

    // If we made it here, we were never preempted. Re-block:
    block_preempt_signal();
}
