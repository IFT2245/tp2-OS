#include "ready_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os.h"

typedef struct node_s {
    process_t*      proc;
    struct node_s*  next;
} node_t;

#define MLFQ_MAX_QUEUES 10

static struct {
    node_t          sentinel;       /* linked list sentinel for many policies */
    size_t          size;
    pthread_mutex_t m;
    pthread_cond_t  c;
    scheduler_alg_t policy;
    node_t          ml_queues[MLFQ_MAX_QUEUES]; /* separate heads for MLFQ levels */
} gQ;

/* Lock helpers */
static void lockQ(void){ pthread_mutex_lock(&gQ.m); }
static void unlockQ(void){ pthread_mutex_unlock(&gQ.m); }

/* Basic FIFO pop from head */
static process_t* pop_head(void){
    while(!gQ.sentinel.next){
        os_log("Ready queue empty, waiting...");
        pthread_cond_wait(&gQ.c, &gQ.m);
    }
    node_t* n = gQ.sentinel.next;
    gQ.sentinel.next = n->next;
    gQ.size--;
    process_t* r = n->proc;
    free(n);
    return r;
}

/* Basic FIFO push to tail */
static void push_tail(process_t* p){
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* c = &gQ.sentinel;
    while(c->next) c = c->next;
    c->next = n;
    gQ.size++;
}

/* Priority push: highest priority first (smaller integer => higher priority if that is the convention) */
static void push_priority(process_t* p){
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* c = &gQ.sentinel;
    while(c->next && p->priority <= c->next->proc->priority){
        c = c->next;
    }
    n->next = c->next;
    c->next = n;
    gQ.size++;
}

/* High Response Ratio Next calculation */
static uint64_t hrrn_calc(const process_t* p, uint64_t now){
    if(!p->burst_time) return 999999UL;
    uint64_t wait = (now > p->arrival_time) ? (now - p->arrival_time) : 0;
    uint64_t r = (p->remaining_time > 0 ? p->remaining_time : 1);
    return (wait + r) / r;
}

/* HRRN push */
static void push_hrrn(process_t* p){
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    uint64_t now = os_time();
    node_t* c = &gQ.sentinel;
    while(c->next){
        uint64_t cval = hrrn_calc(c->next->proc, now);
        uint64_t pval = hrrn_calc(p, now);
        if(pval > cval) break;
        c = c->next;
    }
    n->next = c->next;
    c->next = n;
    gQ.size++;
}

/* CFS push by vruntime ascending */
static void push_cfs(process_t* p){
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* c = &gQ.sentinel;
    while(c->next && p->vruntime >= c->next->proc->vruntime){
        c = c->next;
    }
    n->next = c->next;
    c->next = n;
    gQ.size++;
}

/* SJF push by burst_time ascending */
static void push_sjf(process_t* p){
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* c = &gQ.sentinel;
    while(c->next && p->burst_time >= c->next->proc->burst_time){
        c = c->next;
    }
    n->next = c->next;
    c->next = n;
    gQ.size++;
}

/* MLFQ pop: from topmost queue that has something */
static process_t* pop_mlfq(void){
    for(int i=0; i<MLFQ_MAX_QUEUES; i++){
        if(gQ.ml_queues[i].next){
            node_t* n = gQ.ml_queues[i].next;
            gQ.ml_queues[i].next = n->next;
            gQ.size--;
            process_t* r = n->proc;
            free(n);
            return r;
        }
    }
    return NULL;
}

/* MLFQ push by current level (append) */
static void push_mlfq(process_t* p){
    if(!p) return;
    int lev = p->mlfq_level;
    if(lev < 0) lev = 0;
    if(lev >= MLFQ_MAX_QUEUES) lev = MLFQ_MAX_QUEUES - 1;
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* c = &gQ.ml_queues[lev];
    while(c->next) c = c->next;
    c->next = n;
    gQ.size++;
}

/* Function pointers to selected push/pop per scheduling policy */
static process_t* (*f_pop)(void) = NULL;
static void       (*f_push)(process_t*) = NULL;

void ready_queue_init_policy(scheduler_alg_t alg){
    gQ.sentinel.next = NULL;
    gQ.size = 0;
    pthread_mutex_init(&gQ.m, NULL);
    pthread_cond_init(&gQ.c, NULL);
    gQ.policy = alg;
    for(int i=0; i<MLFQ_MAX_QUEUES; i++){
        gQ.ml_queues[i].next = NULL;
    }
    switch(alg){
    case ALG_FIFO:
    case ALG_RR:
    case ALG_BFS:
        f_push = push_tail;
        f_pop  = pop_head;
        break;
    case ALG_PRIORITY:
        f_push = push_priority;
        f_pop  = pop_head;
        break;
    case ALG_CFS:
    case ALG_CFS_SRTF:
        f_push = push_cfs;
        f_pop  = pop_head;
        break;
    case ALG_SJF:
    case ALG_STRF:
        f_push = push_sjf;
        f_pop  = pop_head;
        break;
    case ALG_HRRN:
    case ALG_HRRN_RT:
        f_push = push_hrrn;
        f_pop  = pop_head;
        break;
    case ALG_MLFQ:
        f_push = push_mlfq;
        f_pop  = pop_mlfq;
        break;
    default:
        f_push = push_tail;
        f_pop  = pop_head;
        break;
    }
    os_log("Ready queue initialized");
}

void ready_queue_destroy(void){
    pthread_cond_destroy(&gQ.c);
    pthread_mutex_destroy(&gQ.m);
    gQ.sentinel.next = NULL;
    for(int i=0; i<MLFQ_MAX_QUEUES; i++){
        gQ.ml_queues[i].next = NULL;
    }
    gQ.size = 0;
    os_log("Ready queue destroyed");
}

void ready_queue_push(process_t* proc){
    lockQ();
    f_push(proc);
    pthread_cond_signal(&gQ.c);
    unlockQ();
}

process_t* ready_queue_pop(void){
    lockQ();
    process_t* r = f_pop();
    while(!r){
        os_log("MLFQ empty queues, waiting...");
        pthread_cond_wait(&gQ.c, &gQ.m);
        r = f_pop();
    }
    unlockQ();
    return r;
}

size_t ready_queue_size(void){
    lockQ();
    size_t s = gQ.size;
    unlockQ();
    return s;
}
