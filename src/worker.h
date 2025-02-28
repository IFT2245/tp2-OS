#ifndef WORKER_H
#define WORKER_H

#include "container.h"
#include "ready_queue.h"
#include <stdlib.h>
#include "scheduler.h"
#include "ready_queue.h"
#include "../lib/log.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

/**
 * @brief A small struct to pass to each core/hpc thread.
 */
typedef struct {
    container_t* container;
    struct {
        ready_queue_t* main_rq;
        ready_queue_t* hpc_rq;
    } qs;
    int core_id;
} core_thread_pack_t;

/**
 * @brief The function each "main core" thread runs.
 */
void* main_core_thread(void* arg);

/**
 * @brief The function each "HPC" thread runs.
 */
void* hpc_thread(void* arg);

/**
 * @brief Bonus HPC BFS test toggle
 */
int is_bonus_test(void);
void set_bonus_test(int onOff);

#endif // WORKER_H
