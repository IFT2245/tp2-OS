#include "scheduler.h"
#include "container.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

/**
 * @brief If compiled with SHELL_TP1_IMPLEMENTATION,
 *        we will do a slow 'sleep(ms)' instead of 'usleep(ms*3000)'.
 *
 *        Adjust as you want for "SLOW CONCURRENCY MOTION".
 */
void do_cpu_work(unsigned long ms, int core_id, int proc_id){
    /* Show concurrency log with color. */
    fprintf(stderr, "\033[36m[CORE %d] Running proc P%d for %lu ms...\033[0m\n",
            core_id, proc_id, ms);

#ifdef SHELL_TP1_IMPLEMENTATION
    /* 1-second granularity: if ms=4 => sleep(4).
       That is a bigger slowdown. Possibly use 'usleep(ms * 1000)' if you prefer. */
    sleep(ms);
#else
    /* Default: multiply by e.g. 1000 or 3000 for a "slow" user experience. */
    usleep((useconds_t)(ms * 3000U));
#endif
}

unsigned long get_quantum(scheduler_alg_t alg, const process_t* p){
    if(!p) return 0;

    switch(alg) {
    case ALG_RR:
        return 2;
    case ALG_BFS:
        return 4;  /* BFS => bigger chunk */
    case ALG_WFQ:
        return 3;
    case ALG_MLFQ:
        /* Example MLFQ: base 2 + 2 per level. */
        return (2 + p->mlfq_level*2);
    case ALG_PRIO_PREEMPT:
        return 2;
    case ALG_FIFO:    /* fallthrough */
    case ALG_SJF:     /* fallthrough */
    case ALG_PRIORITY:/* fallthrough */
    case ALG_HPC:     /* HPC default demonstration */
    case ALG_NONE:    /* fallthrough */
    default:
        /* Fallback => 2. */
        return 2;
    }
}