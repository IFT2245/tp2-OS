#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include <stdbool.h>
#include <pthread.h>
#include "scheduler.h"
#include "process.h"

/* A singly-linked node. */
typedef struct rq_node_s {
    process_t*        proc;
    struct rq_node_s* next;
} rq_node_t;

/* The main "ready queue" structure. */
typedef struct {
    rq_node_t*       head;
    int              size;
    pthread_mutex_t  lock;
    pthread_cond_t   cond;
    scheduler_alg_t  alg;

    double           wfq_virtual_time; /* For Weighted Fair Queueing. */
} ready_queue_t;

/**
 * @brief Initialize a ready queue with a given algorithm.
 */
void rq_init(ready_queue_t* rq, scheduler_alg_t alg);

/**
 * @brief Destroy the queue (frees any leftover nodes).
 */
void rq_destroy(ready_queue_t* rq);

/**
 * @brief Push a process. (If `p == NULL`, we treat that as termination marker.)
 */
void rq_push(ready_queue_t* rq, process_t* p);

/**
 * @brief Pop the next process. If we pop the termination marker, sets *got_term=true.
 */
process_t* rq_pop(ready_queue_t* rq, bool* got_term);

/**
 * @brief Try to see if we need to preempt the current running process
 *        if a higher priority one arrives in the queue (for ALG_PRIO_PREEMPT).
 *
 * @return true if a preemption occurred and we reinserted the old process.
 */
bool try_preempt_if_needed(ready_queue_t* rq, process_t* p);

#endif // READY_QUEUE_H
