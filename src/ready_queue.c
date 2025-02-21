#include "ready_queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "os.h"

typedef struct node_s {
    process_t* proc;
    struct node_s* next;
} node_t;

static struct {
    node_t sentinel;
    size_t size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    scheduler_alg_t policy;
} gQ;

static void lockQ(void){ pthread_mutex_lock(&gQ.mutex); }
static void unlockQ(void){ pthread_mutex_unlock(&gQ.mutex); }

static void push_fifo(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p; n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next) c=c->next;
    c->next=n;
    gQ.size++;
}

static process_t* pop_fifo(void){
    while(!gQ.sentinel.next){
        pthread_cond_wait(&gQ.cond,&gQ.mutex);
    }
    node_t* n=gQ.sentinel.next;
    gQ.sentinel.next=n->next;
    gQ.size--;
    process_t* ret=n->proc;
    free(n);
    return ret;
}

static void push_priority(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p; n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && p->priority <= c->next->proc->priority){
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}

static process_t* pop_priority(void){
    return pop_fifo();
}

static void push_bfs(process_t* p){ push_fifo(p); }
static process_t* pop_bfs(void){ return pop_fifo(); }

static void push_rr(process_t* p){ push_fifo(p); }
static process_t* pop_rr(void){ return pop_fifo(); }

static void push_cfs(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p; n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && p->vruntime >= c->next->proc->vruntime){
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}
static process_t* pop_cfs(void){ return pop_fifo(); }

static void push_cfs_srtf(process_t* p){ push_cfs(p); }
static process_t* pop_cfs_srtf(void){ return pop_fifo(); }

static void push_sjf(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && p->burst_time >= c->next->proc->burst_time) {
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}
static process_t* pop_sjf(void){ return pop_fifo(); }

static void push_strf(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && p->burst_time >= c->next->proc->burst_time) {
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}
static process_t* pop_strf(void){ return pop_fifo(); }

static uint64_t hrrn_calc(process_t* p, uint64_t now){
    uint64_t wait=(now>p->arrival_time)? (now - p->arrival_time):0;
    if(!p->burst_time) return 9999999;
    return (wait + p->burst_time)/p->burst_time;
}
static void push_hrrn(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t));
    n->proc=p;n->next=NULL;
    node_t* c=&gQ.sentinel;
    uint64_t now=os_time();
    while(c->next){
        uint64_t c_val=hrrn_calc(c->next->proc,now);
        uint64_t p_val=hrrn_calc(p,now);
        if(p_val>c_val) break;
        c=c->next;
    }
    n->next=c->next;
    c->next=n;
    gQ.size++;
}
static process_t* pop_hrrn(void){ return pop_fifo(); }

static void push_hrrn_rt(process_t* p){ push_hrrn(p); }
static process_t* pop_hrrn_rt(void){ return pop_fifo(); }

static void (*push_func)(process_t*)=NULL;
static process_t*(*pop_func)(void)=NULL;

void ready_queue_init_policy(scheduler_alg_t alg){
    gQ.sentinel.next=NULL;
    gQ.size=0;
    pthread_mutex_init(&gQ.mutex,NULL);
    pthread_cond_init(&gQ.cond,NULL);
    gQ.policy=alg;

    switch(alg){
    case ALG_FIFO:         push_func=push_fifo;        pop_func=pop_fifo;       break;
    case ALG_RR:           push_func=push_rr;          pop_func=pop_rr;         break;
    case ALG_BFS:          push_func=push_bfs;         pop_func=pop_bfs;        break;
    case ALG_PRIORITY:     push_func=push_priority;    pop_func=pop_priority;   break;
    case ALG_CFS:          push_func=push_cfs;         pop_func=pop_cfs;        break;
    case ALG_CFS_SRTF:     push_func=push_cfs_srtf;    pop_func=pop_cfs_srtf;   break;
    case ALG_SJF:          push_func=push_sjf;         pop_func=pop_sjf;        break;
    case ALG_STRF:         push_func=push_strf;        pop_func=pop_strf;       break;
    case ALG_HRRN:         push_func=push_hrrn;        pop_func=pop_hrrn;       break;
    case ALG_HRRN_RT:      push_func=push_hrrn_rt;     pop_func=pop_hrrn_rt;    break;
    default:               push_func=push_fifo;        pop_func=pop_fifo;       break;
    }
}

void ready_queue_destroy(void){
    pthread_cond_destroy(&gQ.cond);
    pthread_mutex_destroy(&gQ.mutex);
    gQ.sentinel.next=NULL;
    gQ.size=0;
}

void ready_queue_push(process_t* proc){
    if(!proc)return;
    lockQ();
    push_func(proc);
    pthread_cond_signal(&gQ.cond);
    unlockQ();
}

process_t* ready_queue_pop(void){
    lockQ();
    process_t* p=pop_func();
    unlockQ();
    return p;
}

size_t ready_queue_size(void){
    lockQ();
    size_t s=gQ.size;
    unlockQ();
    return s;
}
