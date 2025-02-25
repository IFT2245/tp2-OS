#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include <stddef.h>
#include "process.h"
#include "scheduler.h"

/*
  "ready_queue" data structure, with policy determined by scheduler_alg_t.
  Thread-safe with mutex + cond.
*/

void   ready_queue_init_policy(scheduler_alg_t alg);
void   ready_queue_destroy(void);
void   ready_queue_push(process_t* proc);
process_t* ready_queue_pop(void);
size_t ready_queue_size(void);

#endif
