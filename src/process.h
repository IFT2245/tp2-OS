#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>

/**
 * @brief Represents a single process entity for scheduling.
 */
typedef struct {
    int           id;             /**< Unique ID (for debugging/timeline) */
    unsigned long burst_time;     /**< Original burst (execution) time */
    int           priority;       /**< Used by priority-based schedulers */
    unsigned long arrival_time;   /**< Arrival time for process */
    unsigned long remaining_time; /**< Remaining execution time */
    unsigned long first_response; /**< Timestamp of first scheduling response */
    unsigned long end_time;       /**< Timestamp when process completed */
    bool          responded;      /**< True if process has responded at least once */

    double        weight;         /**< Weight for Weighted Fair Queueing */

    int           hpc_affinity;   /**< HPC thread index if relevant */

    int           mlfq_level;     /**< MLFQ queue level for demonstration */

    bool          was_preempted;  /**< If forcibly preempted */
} process_t;

/**
 * @brief Initialize a single process struct
 */
void init_process(process_t* p, unsigned long burst, int prio, unsigned long arrival, double weight);

#endif
