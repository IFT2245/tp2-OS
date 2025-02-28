#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdbool.h>
#include "../lib/scheduler_alg.h"
#include "process.h"
#include "container.h"

/* Forward-declare container_t for function prototypes: */
struct container_s;
typedef struct container_s container_t;

/**
 * @brief Return the timeslice “quantum” for a given algorithm + process.
 */
unsigned long get_quantum(scheduler_alg_t alg, const process_t* p);

/**
 * @brief Perform actual CPU-bound "work" for `ms` milliseconds,
 *        with optional slow-mode vs. fast-mode behavior.
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

/**
 * @brief Enable or disable slow mode concurrency.
 *        HPC shell is only used if found in ../../libExtern/ ;
 *        otherwise HPC-based tests that rely on it will fail gracefully.
 */
void set_slow_mode(int onOff);

#endif // SCHEDULER_H
