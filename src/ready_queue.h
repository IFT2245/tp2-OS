#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include <stddef.h>
#include "process.h"
#include "scheduler.h"

/*
  "ready_queue" data structure, with policy determined by scheduler_alg_t.
  Thread-safe with mutex + cond.
*/

/* Initialize the ready queue with a given scheduling algorithm. */
void ready_queue_init_policy(scheduler_alg_t alg);

/* Destroy the ready queue, freeing resources. */
void ready_queue_destroy(void);

/* Push a process into the queue. (thread-safe) */
void ready_queue_push(process_t* proc);

/* Pop a process from the queue. (thread-safe, blocks if empty) */
process_t* ready_queue_pop(void);

/* Returns the current size. (thread-safe) */
size_t ready_queue_size(void);

#endif
