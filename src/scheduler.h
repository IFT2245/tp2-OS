#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

typedef enum {
    ALG_CFS,
    ALG_CFS_SRTF,
    ALG_FIFO,
    ALG_RR,
    ALG_SJF,
    ALG_STRF,
    ALG_HRRN,
    ALG_HRRN_RT,
    ALG_BFS,
    ALG_PRIORITY,
    ALG_HPC_OVERSHADOW,
    ALG_MLFQ
} scheduler_alg_t;

/* Select a scheduling algorithm. Next scheduler_run() uses it. */
void scheduler_select_algorithm(scheduler_alg_t alg);

/* Runs the scheduler on the given process list. The chosen scheduling
   algorithm was selected by scheduler_select_algorithm() earlier. */
void scheduler_run(process_t* list, int count);

#endif
