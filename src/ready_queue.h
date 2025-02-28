#ifndef READY_QUEUE_H
#define READY_QUEUE_H
#include <pthread.h>
#include "process.h"
#include "scheduler.h"
#define MLFQ_MAX_QUEUES 10
#define MLFQ_AGING_MS  10
/*
  "ready_queue" data structure, with policy determined by scheduler_alg_t.
  Thread-safe with mutex + cond.
*/
typedef struct node_s {
    process_t*       proc;
    uint64_t         enqueued_sim_time;
    struct node_s*   next;
} node_t;


struct gQ_s {
    node_t           sentinel;
    size_t           size;
    pthread_mutex_t  m;
    pthread_cond_t   c;
    scheduler_alg_t  alg;
    /* The MLFQ queues (0=highest priority, MLFQ_MAX_QUEUES-1=lowest) */
    node_t           ml_queues[MLFQ_MAX_QUEUES];
} gQ;

/* Initialize the ready queue with a given scheduling algorithm. */
void ready_queue_init_policy(scheduler_alg_t alg);

/* Destroy the ready queue, freeing resources. */
void ready_queue_destroy(void);

/* Push a process into the queue. (thread-safe) */
void ready_queue_push(process_t* proc);

/* Pop a process from the queue. (thread-safe, blocks if empty).
   Returns NULL if a sentinel is pushed or if the queue is ended. */
process_t* ready_queue_pop(void);

/* Returns the current size. (thread-safe) */
size_t ready_queue_size(void);


void ready_queue_init2(struct gQ_s* rq, scheduler_alg_t alg);
void ready_queue_destroy2(struct gQ_s* rq);
void ready_queue_push2(struct gQ_s* rq, process_t* proc);
process_t* ready_queue_pop2(struct gQ_s* rq);


#endif
