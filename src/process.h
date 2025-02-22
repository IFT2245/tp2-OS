#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

typedef struct {
    uint64_t burst_time;      /* Original total burst time (ms) */
    int      priority;        /* Priority (lower or higher is 'better' depending on policy) */
    uint64_t vruntime;        /* For CFS / Weighted fair scheduling */
    uint64_t arrival_time;    /* For waiting time calculations */
    uint64_t remaining_time;  /* For partial scheduling */
    uint64_t last_exec;       /* Last time scheduled; can help dynamic priority adjustments */
    int      mlfq_level;      /* Current MLFQ level if MLFQ policy used */
} process_t;

/* Helper function to init a process struct */
void init_process(process_t* p, uint64_t burst, int priority, uint64_t arrival);

#endif
