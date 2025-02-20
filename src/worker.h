#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/*
 * A Worker is a specialized concurrency abstraction
 * that may spawn multiple threads for HPC tasks,
 * handle ephemeral container logic, or pipeline steps,
 * all within a single "worker" instance. We can see it as
 * a "mini-OS" approach inside a process, or a child thread
 * approach inside an HPC node.
 */

typedef uint32_t worker_flags_t;

/* Core concurrency bits (0-4) */
#define WORKER_FLAG_NEW       (1U << 0)
#define WORKER_FLAG_RUNNING   (1U << 1)
#define WORKER_FLAG_DONE      (1U << 2)
#define WORKER_FLAG_ERROR     (1U << 3)
#define WORKER_FLAG_FATAL     (1U << 4)

/* HPC, container, pipeline, etc. (5-9) */
#define WORKER_FLAG_HPC       (1U << 5)
#define WORKER_FLAG_CONTAINER (1U << 6)
#define WORKER_FLAG_PIPELINE  (1U << 7)
#define WORKER_FLAG_DEBUG     (1U << 8)
#define WORKER_FLAG_DISTRIB   (1U << 9)

/* Game modes or advanced bits (10-14) */
#define WORKER_FLAG_GAME_EASY   (1U << 10)
#define WORKER_FLAG_GAME_CHALL  (1U << 11)
#define WORKER_FLAG_GAME_SURVIV (1U << 12)
#define WORKER_FLAG_GAME_BOX    (1U << 13)
#define WORKER_FLAG_GAME_STORY  (1U << 14)

/* Freed up bits or error bits (28-31) */
#define WORKER_FLAG_COMPAT      (1U << 30)
#define WORKER_FLAG_RESERVED    (1U << 31)

typedef struct HPCWorkerThread {
    pthread_t tid;
    long start;
    long end;
    long partial_sum;
    bool active;
} HPCWorkerThread;

typedef struct Worker {
    worker_flags_t flags;

    /* HPC concurrency data: thread count, partial sums, etc. */
    int hpc_thread_count;
    long hpc_total_work;
    long hpc_result;
    HPCWorkerThread* hpc_threads;

    /* Container ephemeral directory. */
    char container_dir[256];
    bool container_ready;

    /* Pipeline stage count, advanced logic. */
    int pipeline_stages;
    int current_stage;

    /* Distribution? Maybe multiple sub threads or sub processes. */
    int distribute_count;

    /* If HPC overshadow or not. */
    pthread_t main_tid;      /* if single main thread spawns HPC sub-threads. */
    bool active;
    bool done;

    /* For advanced game modes: difficulty, score, etc. */
    int game_diff;
    int game_score;
} Worker;

/*
 * Create/destroy:
 */
Worker* worker_create(void);
bool worker_destroy(Worker* w);

/*
 * Configure HPC, container, pipeline, distribution, game modes, etc.
 * Then call worker_start => main thread spawns HPC concurrency, etc.
 * worker_join => wait for completion, gather partial sums, or pipeline done.
 */

bool worker_configure(Worker* w, worker_flags_t flags,
                      int hpc_threads, long total_work,
                      const char* container_path,
                      int pipeline_stages,
                      int distribute_count,
                      int game_diff,
                      int game_score);

bool worker_start(Worker* w);
bool worker_join(Worker* w);
bool worker_cancel(Worker* w);

#endif /* WORKER_H */
