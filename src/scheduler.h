#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdbool.h>
#include "process.h"

typedef enum {
    ALG_NONE=-1,
    ALG_FIFO=0,
    ALG_RR,
    ALG_SJF,
    ALG_PRIORITY,
    ALG_BFS,
    ALG_MLFQ,
    ALG_HPC,
    ALG_WFQ,
    ALG_PRIO_PREEMPT
} scheduler_alg_t;


struct container_s;
typedef struct container_s container_t;

/* Normal scheduler prototypes. */
unsigned long get_quantum(scheduler_alg_t alg, const process_t* p);
void do_cpu_work(unsigned long ms, int core_id, int proc_id);

/* If you also have a function like `record_timeline(container_t*, ...)`,
   forward-declare it here, using the forward-declared container_t.
   Example:
*/
// void record_timeline(container_t* c, int core_id, int proc_id,
//                     unsigned long start_ms, unsigned long slice,
//                     bool preempted_flag);

#endif // SCHEDULER_H
