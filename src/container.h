#ifndef CONTAINER_H
#define CONTAINER_H

#include <pthread.h>
#include <stdbool.h>
#include "process.h"
#include "scheduler.h"

typedef struct timeline_item_s {
    int           core_id;
    int           proc_id;
    unsigned long start_ms;
    unsigned long length_ms;
    bool          preempted_slice;
} timeline_item_t;

void record_timeline(container_t* c,
                     int core_id,
                     int proc_id,
                     unsigned long start_ms,
                     unsigned long slice,
                     bool preempted_flag);

/*
   5) Define the actual container_s struct with references to
      scheduler_alg_t and the timeline array, etc.
*/
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

/* container API: */
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

#endif // CONTAINER_H
