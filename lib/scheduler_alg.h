#ifndef SCHEDULER_ALG_H
#define SCHEDULER_ALG_H

/*
   Standalone definition of the scheduling algorithm enum
*/
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

#endif // SCHEDULER_ALG_H
