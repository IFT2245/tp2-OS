#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "process.h"

/*
  Recognized scheduling algorithms
*/
typedef enum {
    ALG_NONE = -1,
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
    ALG_MLFQ,
    ALG_HPC_OVERLAY,
    ALG_HPC /* HPC placeholder */
} scheduler_alg_t;

/* Forward declaration for container pointer in the callback. */
struct container_s;

/* Callback signature for "process finished" notification. */
typedef void (*finish_cb_t)(struct container_s*);

/* "Run slice" => run partial CPU time on 'p' according to 'alg'.
 * If p finishes => call done_cb(container) */
void scheduler_run_slice(process_t* p, scheduler_alg_t alg,
                         finish_cb_t done_cb,
                         struct container_s* c);


/*
   We store final results of scheduling in a schedule_stats_t.
   This helps keep track of average times, preemptions, HPC modes, etc.
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

    /* If HPC overshadow/overlay => skip normal stats. */
    int HPC_over_mode;
    int HPC_overlay_mode;
} schedule_stats_t;

/*
  A simpler struct to retrieve final scheduling stats
  after a call to scheduler_run().
*/
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

/* Retrieve final stats from last run. */
void scheduler_fetch_report(sched_report_t* out);

/* Access global sim time. Used by ready_queue (e.g. HRRN, MLFQ). */
uint64_t get_global_sim_time(void);

/*
  Provide BFS quantum to the BFS test if BFS is selected.
  If BFS wasn't used, may return leftover or default value.
*/
unsigned long scheduler_get_bfs_quantum(void);

#endif
