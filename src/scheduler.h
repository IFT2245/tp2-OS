#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

#ifdef __cplusplus
extern "C" {
#endif

    /*
      List of supported algorithms.
    */
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

    typedef struct {
        double avg_wait;
        double avg_turnaround;
        double avg_response;
        unsigned long long preemptions;
        unsigned long long total_procs;
    } sched_report_t;

    /* global accessors. */
    void scheduler_select_algorithm(scheduler_alg_t a);
    void scheduler_run(process_t* list, int count);
    void scheduler_fetch_report(sched_report_t* out);

    /* Called by ready_queue so it can do HRRN ratio. */
    uint64_t get_global_sim_time(void);

#ifdef __cplusplus
}
#endif

#endif
