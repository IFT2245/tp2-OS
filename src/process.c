#include <stdlib.h>
#include <string.h>
#include "safe_calls_library.h"
#include "process.h"

/* Initialize a process with given burst, priority, arrival_time. */
void init_process(process_t* p, uint64_t burst, int priority, uint64_t arrival) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->burst_time     = burst;
    p->remaining_time = burst;
    p->priority       = priority;
    p->arrival_time   = arrival;
    p->times_owning_core = 0;
}

/* OPTIONAL new function: reset a process so it can be reused. */
void reset_process(process_t* p, uint64_t new_burst, int new_priority, uint64_t new_arrival) {
    if(!p) return;
    p->burst_time     = new_burst;
    p->remaining_time = new_burst;
    p->priority       = new_priority;
    p->arrival_time   = new_arrival;
    p->vruntime       = 0;
    p->start_time     = 0;
    p->end_time       = 0;
    p->first_response = 0;
    p->responded      = 0;
    p->mlfq_level     = 0;
    p->times_owning_core = 0;
}
