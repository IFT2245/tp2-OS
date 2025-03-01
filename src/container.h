#ifndef CONTAINER_H
#define CONTAINER_H

#include <pthread.h>
#include <stdbool.h>
#include "../lib/scheduler_alg.h"
#include "process.h"
#include "ready_queue.h"
struct core_thread_pack_s;
typedef struct core_thread_pack_s core_thread_pack_t;
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

/**
 * @brief The container struct for processes + HPC threads + scheduling info.
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

    /* NEW FIELD: number of cores actively running a slice right now */
    int   active_cores;

} container_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a container. HPC steal is automatically enabled if nb_cores=0 but main_count>0.
 */
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

/**
 * @brief Run this container in the current thread. (Blocks until completion.)
 */
void container_run(container_t* c);

/**
 * @brief Orchestrator that runs multiple containers in parallel.
 */
void orchestrator_run(container_t* arr, int count);

#endif // CONTAINER_H
