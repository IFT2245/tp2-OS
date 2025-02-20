#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <pthread.h>
#include "os.h"

/*
 * The Process structure will hold a variety of scheduling,
 * concurrency, and "game" information. We incorporate new modes
 * (HPC, container, pipeline, distributed, debug, plus the new game
 * modes) while also storing references for the process-specific data.
 *
 * We'll avoid big in-file commentary. Our aim is to keep code short
 * and direct, with advanced synergy for concurrency + gaming approach.
 */

typedef uint32_t process_flags_t;

/* Basic concurrency flags for process (some might mirror OS): */
#define PROC_FLAG_NEW           (1U << 0)
#define PROC_FLAG_READY         (1U << 1)
#define PROC_FLAG_RUNNING       (1U << 2)
#define PROC_FLAG_BLOCKED       (1U << 3)
#define PROC_FLAG_TERMINATED    (1U << 4)

/* HPC, Container, Pipeline, Dist, Debug, etc. */
#define PROC_FLAG_HPC           (1U << 5)
#define PROC_FLAG_CONTAINER     (1U << 6)
#define PROC_FLAG_PIPELINE      (1U << 7)
#define PROC_FLAG_DISTRIBUTED   (1U << 8)
#define PROC_FLAG_DEBUG         (1U << 9)

/* Scheduling bits (SJF, FIFO, etc.): */
#define PROC_FLAG_SCHED_FIFO    (1U << 10)
#define PROC_FLAG_SCHED_RR      (1U << 11)
#define PROC_FLAG_SCHED_SJF     (1U << 12)
#define PROC_FLAG_SCHED_BFS     (1U << 13)
#define PROC_FLAG_SCHED_PRIO    (1U << 14)

/* Error / special bits: */
#define PROC_FLAG_MULTITHREAD   (1U << 28)
#define PROC_FLAG_ERROR         (1U << 28)
#define PROC_FLAG_FATAL         (1U << 29)
#define PROC_FLAG_COMPAT_BREAK  (1U << 30)
#define PROC_FLAG_RESERVED      (1U << 31)

typedef struct Process {
    process_flags_t flags;       /* concurrency + scheduling + error states */
    pid_t pid;                   /* system PID if multiprocess */
    pthread_t tid;              /* thread handle if needed */
    int priority;               /* for scheduling */
    long hpc_work;              /* HPC total for partial sums, if HPC is set */
    bool completed;             /* if done */

    /* Placeholder for container ephemeral directories, pipeline stage count,
       distributed sub children, or advanced game modes.
       We might unify these with OS modes, but we keep them local to process too. */
    char container_dir[256];
    int pipeline_stages;
    int distributed_count;

    /* HPC partial sum in one process. If HPC is large, we might spawn sub-threads
       but let's keep it simpler at process level. */
    long partial_result;

    /* For advanced "game" approach: track the score or difficulty within each process. */
    int game_score;
    int game_difficulty;

    /* Possibly store references to parent or child processes if we want a tree. */
    struct Process* parent;
} Process;

/*
 * Basic process lifecycle:
 * - process_create: returns a NEW process with default flags
 * - process_configure: sets HPC/container/pipeline flags, scheduling policy, etc.
 * - process_start: if HPC => do HPC partial sums, if container => ephemeral creation, etc.
 * - process_wait: join or wait for child if needed
 * - process_destroy: free the struct
 */

Process* process_create(void);
bool process_configure(Process* proc, process_flags_t flags, int priority,
                       long hpc_work, const char* container_path,
                       int pipeline_stages, int distributed_count,
                       int game_mode_score, int game_mode_diff);

bool process_start(Process* proc);
bool process_wait(Process* proc);
bool process_destroy(Process* proc);

#endif /* PROCESS_H */
