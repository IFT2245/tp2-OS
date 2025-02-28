#ifndef CONTAINER_H
#define CONTAINER_H

#include <pthread.h>
#include <stdbool.h>
#include "../lib/scheduler_alg.h" // for scheduler_alg_t
#include "process.h"

/**
 * Timeline event struct
 */
typedef struct timeline_item_s {
    int           core_id;    /* HPC ID if negative */
    int           proc_id;
    unsigned long start_ms;
    unsigned long length_ms;
    bool          preempted_slice;
} timeline_item_t;

/* Forward declare if needed, but let's define container_s fully. */
typedef struct container_s {
    int              nb_cores;
    int              nb_hpc_threads;
    scheduler_alg_t  main_alg;
    scheduler_alg_t  hpc_alg;

    process_t*       main_procs;
    int              main_count;
    process_t*       hpc_procs;
    int              hpc_count;

    unsigned long    max_cpu_time_ms;
    unsigned long    accumulated_cpu;
    unsigned long    sim_time;
    bool             time_exhausted;
    int              remaining_count;

    pthread_mutex_t  finish_lock;
    pthread_mutex_t  timeline_lock;

    timeline_item_t* timeline;
    int   timeline_count;
    int   timeline_cap;

    char* ephemeral_path;
    bool  allow_hpc_steal;
} container_t;

#ifdef __cplusplus
extern "C" {
#endif

    void container_init(container_t* c,
                        int nb_cores,
                        int nb_hpc_threads,
                        scheduler_alg_t main_alg,
                        scheduler_alg_t hpc_alg,
                        process_t* main_list,
                        int main_count,
                        process_t* hpc_list,
                        int hpc_count,
                        unsigned long max_cpu_ms);

    void container_run(container_t* c);

    void orchestrator_run(container_t* arr, int count);

#ifdef __cplusplus
}
#endif

#endif // CONTAINER_H
