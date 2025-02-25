#include "ready_queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* We'll keep up to 10 MLFQ levels. */
#define MLFQ_MAX_QUEUES 10

typedef struct node_s {
    process_t*       proc;
    struct node_s*   next;
} node_t;

static struct {
    node_t           sentinel;
    size_t           size;
    pthread_mutex_t  m;
    pthread_cond_t   c;
    scheduler_alg_t  alg;
    node_t           ml_queues[MLFQ_MAX_QUEUES];
} gQ;

static pthread_mutex_t* pm(void) {
    return &gQ.m;
}

static pthread_cond_t* pc(void) {
    return &gQ.c;
}

/*
  Remove any 'extern' or forward declarations here;
  'get_global_sim_time' is declared in scheduler.h
*/

/*
 * Linked-list style queue operations for different scheduling policies.
 * We define each push/pop strategy below, then select them in
 * ready_queue_init_policy() based on scheduler_alg_t.
 */

static process_t* pop_head(void) {
    node_t* head = gQ.sentinel.next;
    if (!head) return NULL;
    gQ.sentinel.next = head->next;
    gQ.size--;
    process_t* p = head->proc;
    free(head);
    return p;
}

static void push_tail(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* cur = &gQ.sentinel;
    while (cur->next) {
        cur = cur->next;
    }
    cur->next = n;
    gQ.size++;
}

static void push_priority(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* cur = &gQ.sentinel;
    while (cur->next && (p->priority >= cur->next->proc->priority)) {
        cur = cur->next;
    }
    n->next = cur->next;
    cur->next = n;
    gQ.size++;
}

static void push_cfs(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;

    node_t* cur = &gQ.sentinel;
    while (cur->next && (p->vruntime >= cur->next->proc->vruntime)) {
        cur = cur->next;
    }
    n->next = cur->next;
    cur->next = n;
    gQ.size++;
}

static void push_sjf(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;

    node_t* cur = &gQ.sentinel;
    while (cur->next && (p->burst_time >= cur->next->proc->burst_time)) {
        cur = cur->next;
    }
    n->next = cur->next;
    cur->next = n;
    gQ.size++;
}

/*
 * We'll use a helper function for HRRN ratio: ratio = (wait + remain).
 * Big ratio => process gets scheduled sooner.
 */
static uint64_t hrrn_val(process_t* p, uint64_t now) {
    uint64_t wait   = (now > p->arrival_time) ? (now - p->arrival_time) : 0ULL;
    uint64_t remain = (p->remaining_time > 0) ? p->remaining_time : 1ULL;
    return (wait + remain);
}

static void push_hrrn(process_t* p, int preemptive) {
    (void)preemptive; /* not used directly here for push ordering */

    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;

    uint64_t now = get_global_sim_time();
    uint64_t new_ratio = hrrn_val(p, now);

    node_t* cur = &gQ.sentinel;
    while (cur->next) {
        uint64_t c_ratio = hrrn_val(cur->next->proc, now);
        if (new_ratio > c_ratio) {
            break;
        }
        cur = cur->next;
    }
    n->next = cur->next;
    cur->next = n;
    gQ.size++;
}

static process_t* pop_mlfq(void) {
    for (int i = 0; i < MLFQ_MAX_QUEUES; i++) {
        if (gQ.ml_queues[i].next) {
            node_t* n = gQ.ml_queues[i].next;
            gQ.ml_queues[i].next = n->next;
            gQ.size--;
            process_t* p = n->proc;
            free(n);
            return p;
        }
    }
    return NULL;
}

static void push_mlfq(process_t* p) {
    int level = p->mlfq_level;
    if (level < 0) level = 0;
    if (level >= MLFQ_MAX_QUEUES) level = MLFQ_MAX_QUEUES - 1;

    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;

    node_t* cur = &gQ.ml_queues[level];
    while (cur->next) {
        cur = cur->next;
    }
    cur->next = n;
    gQ.size++;
}

/* The function pointers chosen based on scheduling policy. */
static process_t* (*f_pop)(void) = NULL;
static void       (*f_push)(process_t*) = NULL;
static int        g_preemptive = 0;

/*
 * Public API from ready_queue.h
 */

void ready_queue_init_policy(scheduler_alg_t alg) {
    memset(&gQ, 0, sizeof(gQ));
    pthread_mutex_init(pm(), NULL);
    pthread_cond_init(pc(), NULL);
    gQ.alg = alg;

    switch (alg) {
    case ALG_FIFO:
    case ALG_RR:
    case ALG_BFS:
        f_push = push_tail;
        f_pop  = pop_head;
        g_preemptive = (alg == ALG_RR || alg == ALG_BFS) ? 1 : 0;
        break;

    case ALG_PRIORITY:
        f_push = push_priority;
        f_pop  = pop_head;
        g_preemptive = 0;
        break;

    case ALG_CFS:
        f_push = push_cfs;
        f_pop  = pop_head;
        g_preemptive = 0;
        break;

    case ALG_CFS_SRTF:
        f_push = push_cfs;
        f_pop  = pop_head;
        g_preemptive = 1;
        break;

    case ALG_SJF:
        f_push = push_sjf;
        f_pop  = pop_head;
        g_preemptive = 0;
        break;

    case ALG_STRF:
        f_push = push_sjf;
        f_pop  = pop_head;
        g_preemptive = 1;
        break;

    case ALG_HRRN:
        f_push = (void (*)(process_t*))push_hrrn;
        f_pop  = (process_t* (*)(void))pop_head;
        g_preemptive = 0;
        break;

    case ALG_HRRN_RT:
        f_push = (void (*)(process_t*))push_hrrn;
        f_pop  = (process_t* (*)(void))pop_head;
        g_preemptive = 1;
        break;

    case ALG_MLFQ:
        f_push = (void (*)(process_t*))push_mlfq;
        f_pop  = (process_t* (*)(void))pop_mlfq;
        g_preemptive = 1;
        break;

    default:
        f_push = push_tail;
        f_pop  = pop_head;
        g_preemptive = 0;
        break;
    }
}

void ready_queue_destroy(void) {
    pthread_cond_destroy(pc());
    pthread_mutex_destroy(pm());
    memset(&gQ, 0, sizeof(gQ));
}

void ready_queue_push(process_t* p) {
    pthread_mutex_lock(pm());
    if (p) {
        /* HRRN logic also sets ratio in push step. */
        if (gQ.alg == ALG_HRRN || gQ.alg == ALG_HRRN_RT) {
            push_hrrn(p, (gQ.alg == ALG_HRRN_RT) ? 1 : 0);
        } else if (gQ.alg == ALG_MLFQ) {
            push_mlfq(p);
        } else {
            f_push(p);
        }
    }
    pthread_cond_broadcast(pc());
    pthread_mutex_unlock(pm());
}

process_t* ready_queue_pop(void) {
    pthread_mutex_lock(pm());
    while(1) {
        if (gQ.size > 0) {
            process_t* r = f_pop();
            pthread_mutex_unlock(pm());
            return r;
        }
        pthread_cond_wait(pc(), pm());
    }
    /* unreachable */
    pthread_mutex_unlock(pm());
    return NULL;
}

size_t ready_queue_size(void) {
    pthread_mutex_lock(pm());
    size_t s = gQ.size;
    pthread_mutex_unlock(pm());
    return s;
}
