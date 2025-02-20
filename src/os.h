#ifndef OS_H
#define OS_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t os_flags_t;

/* Core State Bits */
#define OS_FLAG_INITIALIZED        (1U << 0)
#define OS_FLAG_RUNNING            (1U << 1)
#define OS_FLAG_SHUTDOWN           (1U << 2)

/* Concurrency Bits */
#define OS_FLAG_MULTIPROCESS       (1U << 3)
#define OS_FLAG_MULTITHREAD        (1U << 4)

/* Core Feature Modes */
#define OS_FLAG_MODE_HPC           (1U << 5)
#define OS_FLAG_MODE_CONTAINER     (1U << 6)
#define OS_FLAG_MODE_DISTRIBUTED   (1U << 7)
#define OS_FLAG_MODE_DEBUG         (1U << 8)
#define OS_FLAG_MODE_PIPELINE      (1U << 9)

/* Game-Oriented Modes */
#define OS_FLAG_MODE_GAME_EASY     (1U << 10)
#define OS_FLAG_MODE_GAME_CHALLENGE (1U << 11)
#define OS_FLAG_MODE_GAME_SURVIVAL (1U << 12)
#define OS_FLAG_MODE_GAME_SANDBOX  (1U << 13)
#define OS_FLAG_MODE_GAME_STORY    (1U << 14)

/* Scheduling Policies */
#define OS_SCHED_FIFO              (1U << 15)
#define OS_SCHED_RR                (1U << 16)
#define OS_SCHED_SJF               (1U << 17)
#define OS_SCHED_BFS               (1U << 18)
#define OS_SCHED_PRIORITY          (1U << 19)

/* Error and Special Bits */
#define OS_FLAG_ERROR              (1U << 28)
#define OS_FLAG_FATAL              (1U << 29)
#define OS_FLAG_COMPAT_BREAK       (1U << 30)
#define OS_FLAG_RESERVED           (1U << 31)

typedef struct {
    os_flags_t flags;
    int hpc_threads;
    char container_dir[256];
    int distributed_count;
    int pipeline_stages;
    long hpc_result;
    int sched_quantum_ms;
    int game_score;
    int game_difficulty;
} OSCore;

OSCore* os_init(void);
bool os_run(OSCore* core);
bool os_shutdown(OSCore* core);
void os_ui_interact(OSCore* core);

#endif
