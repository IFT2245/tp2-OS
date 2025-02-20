#ifndef READY_H
#define READY_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

/*
 * We represent scheduling and concurrency flags for the "ready queue."
 * We'll incorporate advanced concurrency (multi-process, multi-thread),
 * plus game-based modes, HPC logic, container tasks, pipeline, etc.
 */
typedef uint32_t ready_flags_t;

/* Scheduling policies (bits 0-4) */
#define RQ_SCHED_FIFO        (1U << 0)
#define RQ_SCHED_RR          (1U << 1)
#define RQ_SCHED_SJF         (1U << 2)
#define RQ_SCHED_BFS         (1U << 3)
#define RQ_SCHED_PRIORITY    (1U << 4)

/* Concurrency / game / advanced flags (bits 5-14) */
#define RQ_MODE_HPC          (1U << 5)
#define RQ_MODE_CONTAINER    (1U << 6)
#define RQ_MODE_PIPELINE     (1U << 7)
#define RQ_MODE_DISTRIBUTED  (1U << 8)
#define RQ_MODE_DEBUG        (1U << 9)
#define RQ_MODE_GAME_EASY    (1U << 10)
#define RQ_MODE_GAME_CHALL   (1U << 11)
#define RQ_MODE_GAME_SURVIV  (1U << 12)
#define RQ_MODE_GAME_SANDBOX (1U << 13)
#define RQ_MODE_GAME_STORY   (1U << 14)

/* Error or special bits (28-31) */
#define RQ_FLAG_ERROR        (1U << 28)
#define RQ_FLAG_FATAL        (1U << 29)
#define RQ_FLAG_COMPAT       (1U << 30)
#define RQ_FLAG_RESERVED     (1U << 31)

/*
 * The ReadyQueue is a container of processes. We maintain
 * an array or list of Process*, plus concurrency and scheduling flags.
 */
typedef struct ReadyQueue {
    ready_flags_t flags;
    int capacity;
    int size;
    Process** procs;

    /* Extended data for scheduling approach (RR quantum, etc.) */
    int sched_quantum_ms;

    /* HPC concurrency or container usage at queue level, game modes, etc. */
    int default_hpc_threads;
    int default_distributed_count;
    int default_pipeline_stages;

    /* Score or difficulty if we embed game logic at queue level. */
    int game_score;
    int game_difficulty;
} ReadyQueue;

/*
 * Basic queue API:
 *  - ready_create
 *  - ready_configure
 *  - ready_enqueue / ready_dequeue
 *  - ready_run => runs scheduling logic or simply starts/ waits each process
 *  - ready_destroy
 */

ReadyQueue* ready_create(int capacity_hint);
bool ready_configure(ReadyQueue* rq);
bool ready_enqueue(ReadyQueue* rq, Process* proc);
Process* ready_dequeue(ReadyQueue* rq);
bool ready_run(ReadyQueue* rq);
bool ready_destroy(ReadyQueue* rq);

/*
 * Optional UI function to manage the queue in an interactive or game-based approach.
 */
void ready_ui_interact(ReadyQueue* rq);

#endif /* READY_H */
