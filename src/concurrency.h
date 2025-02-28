#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <pthread.h>
#include <stdbool.h>

/**
 * @brief Enumeration representing different scheduling algorithms.
 */
typedef enum {
    ALG_NONE=-1,
    ALG_FIFO=0,
    ALG_RR,
    ALG_SJF,
    ALG_PRIORITY,
    ALG_BFS,
    ALG_MLFQ,
    ALG_HPC,
    ALG_WFQ,           /* Weighted Fair Queueing demonstration */
    ALG_PRIO_PREEMPT   /* New: Preemptive Priority scheduling */
} scheduler_alg_t;

/**
 * @brief Represents a single process entity for scheduling.
 */
typedef struct {
    int           id;             /**< Unique ID for debugging/timeline */
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

    bool          was_preempted;  /**< If forcibly preempted (for logging) */
} process_t;

/**
 * @brief A container that includes resources for:
 * - multiple CPU core threads
 * - multiple HPC (high-performance) threads
 * - separate scheduling algorithms for main vs HPC
 * - a timeline of scheduling events
 */
typedef struct container_s {
    int              nb_cores;         /**< Number of main cores */
    int              nb_hpc_threads;   /**< Number of HPC threads */
    scheduler_alg_t  main_alg;         /**< Algorithm for main queue */
    scheduler_alg_t  hpc_alg;          /**< Algorithm for HPC queue */

    process_t*       main_procs;       /**< Array of main processes */
    int              main_count;
    process_t*       hpc_procs;
    int              hpc_count;

    unsigned long    max_cpu_time_ms;  /**< Hard limit on CPU usage (simulation ends if exceeded) */
    unsigned long    accumulated_cpu;  /**< Sum of CPU time used so far */
    unsigned long    sim_time;         /**< "Current" simulation time */
    bool             time_exhausted;   /**< If we have ended the simulation */

    int              remaining_count;  /**< #processes not finished */

    pthread_mutex_t  timeline_lock;
    struct {
        int           core_id;    /* HPC ID if negative */
        int           proc_id;
        unsigned long start_ms;
        unsigned long length_ms;
        bool          preempted_slice;
    } *timeline;
    int   timeline_count;
    int   timeline_cap;

    char* ephemeral_path;

    pthread_mutex_t finish_lock;

    bool allow_hpc_steal; /**< HPC can steal from main if no HPC tasks. */
} container_t;


/**
 * @brief Initialize a single process struct.
 */
void init_process(process_t* p, unsigned long burst, int prio, unsigned long arrival, double weight);

/**
 * @brief Create + init a container for scheduling.
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
 * @brief Start multiple containers in parallel.
 *        (Each container spawns threads internally.)
 */
void orchestrator_run(container_t* arr, int count);

#endif
