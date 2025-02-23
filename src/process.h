#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

/*
  Represents a single "process" or "task" in our scheduler.
  We'll track the original burst_time, priority, arrival_time,
  plus some scheduling fields (vruntime, remaining_time, start_time, end_time, etc.).
*/

typedef struct process_s {
    uint64_t burst_time;       /* how long the process takes (ms) total */
    int      priority;         /* smaller => higher priority (in some algs) */
    uint64_t vruntime;         /* used by CFS */
    uint64_t arrival_time;     /* simulation arrival time */
    uint64_t remaining_time;   /* how many ms remain for this process */

    /* Additional fields to track stats: */
    uint64_t start_time;       /* first time it's scheduled (sim time) */
    uint64_t end_time;         /* finish time (sim time) */
    uint64_t first_response;   /* first time on CPU (sim time) */
    int      responded;        /* 0 if not responded yet, 1 if yes */

    int      mlfq_level;       /* which queue level for MLFQ */
    int      times_owning_core;/* how many times scheduled so far */
} process_t;

/* Initialize a process with the given burst, priority, arrival_time. */
void init_process(process_t* p, uint64_t burst, int priority, uint64_t arrival);

#endif
