#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

/*
  Single "process" (or "task") structure for scheduling.
  Fields:
   - burst_time
   - priority
   - vruntime
   - arrival_time
   - remaining_time
   - stats (start_time, end_time, first_response, responded)
   - MLFQ queue level
   - times_owning_core
*/

typedef struct process_s {
    uint64_t burst_time;       /* total CPU time needed (ms) */
    int      priority;         /* smaller => higher priority (for some algs) */
    uint64_t vruntime;         /* used by CFS / CFS-SRTF */
    uint64_t arrival_time;     /* simulation "arrival" time */
    uint64_t remaining_time;   /* how many ms remain for this process */

    /* Additional fields to track stats: */
    uint64_t start_time;
    uint64_t end_time;
    uint64_t first_response;
    int      responded;
    int      mlfq_level;
    int      times_owning_core;
} process_t;

/* Initialize a process with the given burst, priority, arrival_time. */
void init_process(process_t* p, uint64_t burst, int priority, uint64_t arrival);

#endif
