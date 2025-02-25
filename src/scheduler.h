#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "process.h"

/*
  Recognized scheduling algorithms
*/
typedef enum {
    ALG_CFS = 0,
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

/*
   We store final results of scheduling in a schedule_stats_t.
   This helps keep track of average times, preemptions, HPC mode, etc.
*/
typedef struct {
    double avg_wait;
    double avg_turnaround;
    double avg_response;
    unsigned long long preemptions;
    unsigned long long total_procs;

    /* Internal aggregator: sums for final stats. */
    uint64_t total_wait;
    uint64_t total_tat;
    uint64_t total_resp;
    unsigned long long total_preempts;
    int total_count;

    /* Flag if HPC overshadow mode => skip normal stats. */
    int HPC_over_mode;
} schedule_stats_t;

/* A simpler struct to retrieve final scheduling stats. */
typedef struct {
    double avg_wait;
    double avg_turnaround;
    double avg_response;
    unsigned long long preemptions;
    unsigned long long total_procs;
} sched_report_t;

/* Select an algorithm. */
void scheduler_select_algorithm(scheduler_alg_t a);

/* Run the scheduling simulation on an array of processes. */
void scheduler_run(process_t* list, int count);

/* Retrieve final stats from last run. HPC overshadow => zeroed stats. */
void scheduler_fetch_report(sched_report_t* out);

/* Accessor so the ready-queue can see current simulated time for HRRN, etc. */
uint64_t get_global_sim_time(void);

#endif
