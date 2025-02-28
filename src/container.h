#ifndef CONTAINER_H
#define CONTAINER_H

#include <stdbool.h>
#include <pthread.h>
#include "scheduler.h"
#include "process.h"
#include "ready_queue.h"

/*
 * container.h
 * A container that can have:
 *  - main_queue for normal processes
 *  - HPC queue if hpc_enabled == true
 *
 * We removed overshadow references; HPC is just "ALG_HPC" if you want HPC.
 */

typedef struct container_s {
    int               nb_cores;
    bool              hpc_enabled;
    scheduler_alg_t   sched_main;
    scheduler_alg_t   sched_hpc;
    char*             ephemeral_path;
    process_t*        main_procs;
    int               main_count;

    process_t*        hpc_procs;
    int               hpc_count;

    struct gQ_s       main_queue;
    struct gQ_s       hpc_queue;

    pthread_mutex_t   finish_lock;
    int               finished_main;
    int               finished_hpc;
} container_t;

/* Initialize container with the given scheduling algs, HPC optional. */
void container_init(container_t* c,
                    int nb_cores,
                    bool hpc_enabled,
                    scheduler_alg_t main_alg,
                    scheduler_alg_t hpc_alg,
                    process_t* main_list, int main_count,
                    process_t* hpc_list,  int hpc_count);

/* container_run => run this container's processes in parallel threads. */
void* container_run(void* arg);

#endif
