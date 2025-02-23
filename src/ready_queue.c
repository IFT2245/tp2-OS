#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "ready_queue.h"
#include "os.h"

/* Implementation of a multi-level structure for MLFQ, or a single queue, etc. */

#define MLFQ_MAX_QUEUES 10

typedef struct node_s {
    process_t*     proc;
    struct node_s* next;
} node_t;

static struct {
    node_t sentinel;
    size_t size;
    pthread_mutex_t m;
    pthread_cond_t  c;
    scheduler_alg_t alg;
    node_t ml_queues[MLFQ_MAX_QUEUES]; /* for MLFQ usage */
} gQ;

static pthread_mutex_t* pm(void){ return &gQ.m; }
static pthread_cond_t*  pc(void){ return &gQ.c; }

/* For non-MLFQ, we store all in sentinel -> next chain. */

static process_t* pop_head(void){
    while(!gQ.sentinel.next){
        pthread_cond_wait(pc(), pm());
    }
    node_t* n=gQ.sentinel.next;
    gQ.sentinel.next=n->next;
    gQ.size--;
    process_t* r=n->proc;
    free(n);
    return r;
}

/* Insert at tail for FIFO, RR, BFS, etc. */
static void push_tail(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;
    n->next=NULL;

    node_t* c=&gQ.sentinel;
    while(c->next) c=c->next;
    c->next=n;
    gQ.size++;
}

/* Insert by priority (smaller priority => schedule first). */
static void push_prio(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;
    n->next=NULL;

    node_t* c=&gQ.sentinel;
    while(c->next && (p->priority >= c->next->proc->priority)){
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}

/* Insert by vruntime ascending. */
static void push_cfs(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;
    n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && (p->vruntime >= c->next->proc->vruntime)){
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}

/* Insert by burst_time ascending => SJF. */
static void push_sjf(process_t* p){
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->next = NULL;
    node_t* c = &gQ.sentinel;
    while(c->next && (p->burst_time >= c->next->proc->burst_time)){
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}

/* Weighted ratio => HRRN => we approximate using (wait+remain)/remain. We'll do the push in descending order. */
static uint64_t hrrn_val(process_t* p, uint64_t current_sim_time){
    uint64_t wait = (current_sim_time > p->arrival_time)
                    ? (current_sim_time - p->arrival_time)
                    : 0ULL;
    uint64_t r = (p->remaining_time>0 ? p->remaining_time : 1);
    /* ratio = (wait + r)/r => we store scaled ratio in integer. */
    return (wait + r);
}

static void push_hrrn(process_t* p){
    /* We'll place p in the queue so that if p has bigger ratio => earlier in the list. */
    extern uint64_t get_global_sim_time(void);
    uint64_t now = get_global_sim_time();
    uint64_t new_val = hrrn_val(p, now);

    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;
    n->next=NULL;

    node_t* c=&gQ.sentinel;
    while(c->next){
        uint64_t cv = hrrn_val(c->next->proc, now);
        if(new_val > cv){
            /* bigger ratio => insert here. */
            break;
        }
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}

/* MLFQ => pop from the highest priority queue that has something. */
static process_t* pop_mlfq(void){
    for(int i=0;i<MLFQ_MAX_QUEUES;i++){
        if(gQ.ml_queues[i].next){
            /* pop head from queue i */
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

/* push to MLFQ queue based on p->mlfq_level. */
static void push_mlfq(process_t* p){
    if(!p) return;
    int lev = p->mlfq_level;
    if(lev < 0) lev=0;
    if(lev >= MLFQ_MAX_QUEUES) lev=MLFQ_MAX_QUEUES-1;

    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;
    n->next=NULL;

    node_t* c=&gQ.ml_queues[lev];
    while(c->next) c=c->next;
    c->next=n;
    gQ.size++;
}

/* We'll keep function pointers based on the chosen alg. */
static process_t* (*f_pop)(void)=NULL;
static void       (*f_push)(process_t*)=NULL;

void ready_queue_init_policy(scheduler_alg_t alg){
    memset(&gQ, 0, sizeof(gQ));
    pthread_mutex_init(pm(), NULL);
    pthread_cond_init(pc(), NULL);
    gQ.alg = alg;

    switch(alg){
    case ALG_FIFO:
    case ALG_RR:
    case ALG_BFS:
        f_push = push_tail;
        f_pop  = pop_head;
        break;
    case ALG_PRIORITY:
        f_push = push_prio;
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
}

void ready_queue_destroy(void){
    pthread_cond_destroy(pc());
    pthread_mutex_destroy(pm());
    memset(&gQ, 0, sizeof(gQ));
}

void ready_queue_push(process_t* p){
    pthread_mutex_lock(pm());
    f_push(p);
    pthread_cond_signal(pc());
    pthread_mutex_unlock(pm());
}

process_t* ready_queue_pop(void){
    pthread_mutex_lock(pm());
    process_t* r=f_pop();
    while(!r){
        pthread_cond_wait(pc(), pm());
        r=f_pop();
    }
    pthread_mutex_unlock(pm());
    return r;
}

size_t ready_queue_size(void){
    pthread_mutex_lock(pm());
    size_t s=gQ.size;
    pthread_mutex_unlock(pm());
    return s;
}
