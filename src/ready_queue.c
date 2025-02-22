#include "ready_queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "os.h"

#define MLFQ_MAX_QUEUES 10

typedef struct node_s {
    process_t* proc;
    struct node_s* next;
} node_t;

static struct {
    node_t sentinel;
    size_t size;
    pthread_mutex_t m;
    pthread_cond_t c;
    scheduler_alg_t alg;
    node_t ml_queues[MLFQ_MAX_QUEUES];
} gQ;

static pthread_mutex_t* pm(void){return &gQ.m;}
static pthread_cond_t* pc(void){return &gQ.c;}

static process_t* pop_head(void){
    while(!gQ.sentinel.next) pthread_cond_wait(pc(),pm());
    node_t* n=gQ.sentinel.next; gQ.sentinel.next=n->next; gQ.size--;
    process_t* r=n->proc; free(n); return r;
}
static void push_tail(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t)); n->proc=p; n->next=NULL;
    node_t* c=&gQ.sentinel; while(c->next) c=c->next; c->next=n; gQ.size++;
}
static void push_prio(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t)); n->proc=p; n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && p->priority<=c->next->proc->priority) c=c->next;
    n->next=c->next; c->next=n; gQ.size++;
}
static void push_cfs(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t)); n->proc=p; n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && p->vruntime>=c->next->proc->vruntime) c=c->next;
    n->next=c->next; c->next=n; gQ.size++;
}
static void push_sjf(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t)); n->proc=p; n->next=NULL;
    node_t* c=&gQ.sentinel;
    while(c->next && p->burst_time>=c->next->proc->burst_time) c=c->next;
    n->next=c->next; c->next=n; gQ.size++;
}
static uint64_t hrrn_val(process_t* p,uint64_t now){ if(!p->burst_time)return 999999; uint64_t w=(now>p->arrival_time)?(now-p->arrival_time):0; uint64_t r=(p->remaining_time>0?p->remaining_time:1); return (w+r)/r; }
static void push_hrrn(process_t* p){
    node_t* n=(node_t*)malloc(sizeof(node_t)); n->proc=p; n->next=NULL;
    uint64_t now=os_time(); node_t* c=&gQ.sentinel;
    while(c->next){
        uint64_t cv=hrrn_val(c->next->proc,now), pv=hrrn_val(p,now);
        if(pv>cv) break; c=c->next;
    }
    n->next=c->next; c->next=n; gQ.size++;
}
static process_t* pop_mlfq(void){
    for(int i=0;i<MLFQ_MAX_QUEUES;i++){
        if(gQ.ml_queues[i].next){
            node_t* n=gQ.ml_queues[i].next; gQ.ml_queues[i].next=n->next; gQ.size--;
            process_t* r=n->proc; free(n); return r;
        }
    }
    return NULL;
}
static void push_mlfq(process_t* p){
    if(!p)return; int lev=p->mlfq_level; if(lev<0)lev=0; if(lev>=MLFQ_MAX_QUEUES) lev=MLFQ_MAX_QUEUES-1;
    node_t* n=(node_t*)malloc(sizeof(node_t)); n->proc=p; n->next=NULL;
    node_t* c=&gQ.ml_queues[lev]; while(c->next) c=c->next;
    c->next=n; gQ.size++;
}

static process_t*(*f_pop)(void)=NULL;
static void (*f_push)(process_t*)=NULL;

void ready_queue_init_policy(scheduler_alg_t alg){
    memset(&gQ,0,sizeof(gQ));
    pthread_mutex_init(pm(),NULL);
    pthread_cond_init(pc(),NULL);
    gQ.alg=alg;
    switch(alg){
    case ALG_FIFO:
    case ALG_RR:
    case ALG_BFS:
        f_push=push_tail; f_pop=pop_head; break;
    case ALG_PRIORITY:
        f_push=push_prio; f_pop=pop_head; break;
    case ALG_CFS:
    case ALG_CFS_SRTF:
        f_push=push_cfs; f_pop=pop_head; break;
    case ALG_SJF:
    case ALG_STRF:
        f_push=push_sjf; f_pop=pop_head; break;
    case ALG_HRRN:
    case ALG_HRRN_RT:
        f_push=push_hrrn; f_pop=pop_head; break;
    case ALG_MLFQ:
        f_push=push_mlfq; f_pop=pop_mlfq; break;
    default:
        f_push=push_tail; f_pop=pop_head; break;
    }
}
void ready_queue_destroy(void){
    pthread_cond_destroy(pc());
    pthread_mutex_destroy(pm());
    memset(&gQ,0,sizeof(gQ));
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
        pthread_cond_wait(pc(),pm());
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
