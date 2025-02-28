#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <unistd.h>
#include "../lib/scheduler_alg.h"
#include "process.h"

/* Forward-declare container_t so we can have record_timeline(...) prototype
   without including container.h (prevents circular includes). */
struct container_s;
typedef struct container_s container_t;

/**
 * @brief Return the timeslice “quantum” for a given algorithm + process.
 */
unsigned long get_quantum(scheduler_alg_t alg, const process_t* p);

/**
 * @brief Perform actual CPU-bound "work" for `ms` milliseconds,
 *        with a reduced slowdown so HPC BFS won't time out quickly.
 */
void do_cpu_work(unsigned long ms, int core_id, int proc_id);

/**
 * @brief Record a timeline event into the container's timeline array.
 */
void record_timeline(container_t* c,
                     int core_id,
                     int proc_id,
                     unsigned long start_ms,
                     unsigned long slice,
                     bool preempted_flag);

#endif // SCHEDULER_H
