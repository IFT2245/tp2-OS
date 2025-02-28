#include "ready_queue.h"
#include "scheduler.h"
#include "process.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/*
   The global queue structure with:
   - sentinel for normal single-queue algorithms
   - ml_queues[] for MLFQ
   - size
   - chosen alg
   - locks/conds
*/

/* Helper for mutex/cond. */
static pthread_mutex_t* pm(void) { return &gQ.m; }
static pthread_cond_t*  pc(void) { return &gQ.c; }

/* ---------- Linked-list queue basics ---------- */
static process_t* pop_head(void) {
    node_t* head = gQ.sentinel.next;
    if (!head) return NULL;
    gQ.sentinel.next = head->next;
    gQ.size--;

    /* If this is a sentinel node => return NULL so the thread can exit. */
    process_t* p = head->proc;
    free(head);
    return p;
}

static void push_tail(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->enqueued_sim_time = get_global_sim_time();
    n->next = NULL;
    node_t* cur = &gQ.sentinel;
    while (cur->next) {
        cur = cur->next;
    }
    cur->next = n;
    gQ.size++;
}

/* For priority-based insertion. */
static void push_priority(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->enqueued_sim_time = get_global_sim_time();
    n->next = NULL;

    node_t* cur = &gQ.sentinel;
    /* smaller priority => earlier in the list */
    while (cur->next && (p->priority >= cur->next->proc->priority)) {
        cur = cur->next;
    }
    n->next = cur->next;
    cur->next = n;
    gQ.size++;
}

/* For CFS => insert by ascending vruntime. */
static void push_cfs(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->enqueued_sim_time = get_global_sim_time();
    n->next = NULL;

    node_t* cur = &gQ.sentinel;
    while (cur->next && (p->vruntime >= cur->next->proc->vruntime)) {
        cur = cur->next;
    }
    n->next = cur->next;
    cur->next = n;
    gQ.size++;
}

/* For SJF => insert by ascending burst_time. */
static void push_sjf(process_t* p) {
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->enqueued_sim_time = get_global_sim_time();
    n->next = NULL;

    node_t* cur = &gQ.sentinel;
    while (cur->next && (p->burst_time >= cur->next->proc->burst_time)) {
        cur = cur->next;
    }
    n->next = cur->next;
    cur->next = n;
    gQ.size++;
}

/* Helper for HRRN ratio: (wait + remain). Higher => schedule first. */
static uint64_t hrrn_val(const process_t* p, const uint64_t now) {
    const uint64_t wait   = (now > p->arrival_time) ? (now - p->arrival_time) : 0ULL;
    const uint64_t remain = (p->remaining_time > 0) ? p->remaining_time : 1ULL;
    return (wait + remain);
}

static void push_hrrn(process_t* p, int preemptive) {
    (void)preemptive; /* same insertion logic; preemption is handled in scheduling loop */
    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->enqueued_sim_time = get_global_sim_time();
    n->next = NULL;

    uint64_t now = get_global_sim_time();
    uint64_t new_ratio = hrrn_val(p, now);

    node_t* cur = &gQ.sentinel;
    while (cur->next && cur->next->proc != NULL) {
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

/* ---------- MLFQ ---------- */
static process_t* pop_mlfq(void) {
    for (int i = 0; i < MLFQ_MAX_QUEUES; i++) {
        if (gQ.ml_queues[i].next) {
            node_t* n = gQ.ml_queues[i].next;
            gQ.ml_queues[i].next = n->next;
            gQ.size--;

            /* If it's a sentinel node => return NULL so the core thread ends. */
            process_t* p = n->proc;
            free(n);
            return p;
        }
    }
    return NULL;
}

static void push_mlfq(process_t* p) {
    /* If the process is actually the sentinel => put it in queue 0 so it can pop easily. */
    int level = (p ? p->mlfq_level : 0);
    if (level < 0) level = 0;
    if (level >= MLFQ_MAX_QUEUES) level = MLFQ_MAX_QUEUES - 1;

    node_t* n = (node_t*)malloc(sizeof(node_t));
    n->proc = p;
    n->enqueued_sim_time = get_global_sim_time();
    n->next = NULL;

    node_t* cur = &gQ.ml_queues[level];
    while (cur->next) {
        cur = cur->next;
    }
    cur->next = n;
    gQ.size++;
}

/*
   Each time we pop in MLFQ, we do a quick pass to check
   if any process in a lower queue has waited >= MLFQ_AGING_MS
   => promote it up one level.
*/
static void mlfq_promote_aged_processes(void) {
    uint64_t now = get_global_sim_time();
    for(int level = 1; level < MLFQ_MAX_QUEUES; level++) {
        node_t* head = &gQ.ml_queues[level];
        node_t* cur  = head;
        while(cur->next) {
            node_t* nxt = cur->next;
            if(nxt->proc == NULL) {
                /* skip sentinel nodes if any ended up here */
                cur = cur->next;
                continue;
            }
            uint64_t waited = (now > nxt->enqueued_sim_time)
                               ? (now - nxt->enqueued_sim_time)
                               : 0ULL;
            if(waited >= MLFQ_AGING_MS) {
                /* remove from current queue */
                cur->next = nxt->next;
                gQ.size--;

                /* promote up one level */
                int newLevel = level - 1;
                if(newLevel<0) newLevel=0;
                nxt->proc->mlfq_level = newLevel;
                nxt->enqueued_sim_time = now;

                /* push it to the newLevel queue at the tail */
                node_t* tail = &gQ.ml_queues[newLevel];
                while(tail->next) {
                    tail = tail->next;
                }
                tail->next = nxt;
                nxt->next  = NULL;
                gQ.size++;

                /* do not advance cur => we removed nxt from chain. */
            } else {
                cur = cur->next;
            }
        }
    }
}

/* ---------- Public interface ---------- */
void ready_queue_init_policy(scheduler_alg_t alg) {
    memset(&gQ, 0, sizeof(gQ));
    pthread_mutex_init(pm(), NULL);
    pthread_cond_init(pc(), NULL);
    gQ.alg = alg;
}

/* We leave the function pointers approach inlined to keep code simpler. */
void ready_queue_destroy(void) {
    pthread_cond_destroy(pc());
    pthread_mutex_destroy(pm());
    memset(&gQ, 0, sizeof(gQ));
}

/*
   FIX: If p==NULL => we still push a *sentinel node*
   so that waiting threads can pop it and exit.
*/
void ready_queue_push(process_t* p) {
    pthread_mutex_lock(pm());

    /* If 'p' is NULL => push sentinel node so pop() can return NULL. */
    if(!p) {
        node_t* n = (node_t*)malloc(sizeof(node_t));
        n->proc = NULL; /* sentinel */
        n->enqueued_sim_time = get_global_sim_time();
        n->next = NULL;

        /* For MLFQ, place sentinel in queue 0 so it can be seen immediately. */
        if(gQ.alg == ALG_MLFQ) {
            node_t* cur = &gQ.ml_queues[0];
            while(cur->next) {
                cur = cur->next;
            }
            cur->next = n;
            gQ.size++;
        } else {
            /* For single-queue algs => put it in the main sentinel list. */
            node_t* cur = &gQ.sentinel;
            while(cur->next) {
                cur = cur->next;
            }
            cur->next = n;
            gQ.size++;
        }
        /* Wake up any waiting threads. */
        pthread_cond_broadcast(pc());
        pthread_mutex_unlock(pm());
        return;
    }

    /* If it's HRRN: */
    if (gQ.alg == ALG_HRRN || gQ.alg == ALG_HRRN_RT) {
        uint64_t now = get_global_sim_time();
        uint64_t new_ratio = hrrn_val(p, now);

        node_t* n = (node_t*)malloc(sizeof(node_t));
        n->proc = p;
        n->enqueued_sim_time = now;
        n->next = NULL;

        node_t* cur = &gQ.sentinel;
        while (cur->next && cur->next->proc != NULL) {
            uint64_t c_ratio = hrrn_val(cur->next->proc, now);
            if (new_ratio > c_ratio) break;
            cur = cur->next;
        }
        n->next = cur->next;
        cur->next = n;
        gQ.size++;
    }
    else if(gQ.alg == ALG_MLFQ) {
        push_mlfq(p);
    }
    else {
        /* For simpler algs => do insertion logic depending on which one it is. */
        /* We can detect it from gQ.alg if we want the original approach. */
        switch(gQ.alg) {
            case ALG_FIFO:
            case ALG_RR:
            case ALG_BFS:
                push_tail(p);
                break;
            case ALG_PRIORITY:
                push_priority(p);
                break;
            case ALG_CFS:
            case ALG_CFS_SRTF:
                push_cfs(p);
                break;
            case ALG_SJF:
            case ALG_STRF:
                push_sjf(p);
                break;
            default:
                push_tail(p);
                break;
        }
    }

    pthread_cond_broadcast(pc());
    pthread_mutex_unlock(pm());
}

/*
   The caller blocks here if the queue is empty.
   If we see a sentinel node => return NULL, causing the scheduling thread to exit.
*/
process_t* ready_queue_pop(void) {
    pthread_mutex_lock(pm());
    while(1) {
        if (gQ.size > 0) {
            if(gQ.alg == ALG_MLFQ) {
                /* MLFQ: do a quick check for aging before we pop. */
                mlfq_promote_aged_processes();
                process_t* p = pop_mlfq();
                pthread_mutex_unlock(pm());

                /* If sentinel => return NULL => thread exit. */
                if(!p) {
                    return NULL;
                }
                return p;
            }
            else {
                /* Single queue approach. Check if the front is sentinel. */
                node_t* front = gQ.sentinel.next;
                if(front && front->proc == NULL) {
                    /* pop the sentinel => return NULL => exit. */
                    gQ.sentinel.next = front->next;
                    gQ.size--;
                    free(front);
                    pthread_mutex_unlock(pm());
                    return NULL;
                }
                /* Otherwise pop normally. */
                process_t* p = pop_head();
                pthread_mutex_unlock(pm());
                return p;
            }
        }
        /* If empty => wait. */
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


static process_t* pop_head2(struct gQ_s* rq){
  node_t* h=rq->sentinel.next;if(!h)return NULL;rq->sentinel.next=h->next;rq->size--;process_t* p=h->proc;free(h);return p;
}
void ready_queue_init2(struct gQ_s* rq,scheduler_alg_t a){memset(rq,0,sizeof(*rq));pthread_mutex_init(&rq->m,NULL);pthread_cond_init(&rq->c,NULL);rq->alg=a;}
void ready_queue_destroy2(struct gQ_s* rq){node_t* c=rq->sentinel.next;while(c){node_t* t=c;c=c->next;free(t);}pthread_mutex_destroy(&rq->m);pthread_cond_destroy(&rq->c);memset(rq,0,sizeof(*rq));}
void push_fifo2(struct gQ_s* rq,process_t* p){node_t* n=(node_t*)malloc(sizeof(node_t));n->proc=p;n->next=NULL;node_t* c=&rq->sentinel;while(c->next)c=c->next;c->next=n;rq->size++;}
void push_priority2(struct gQ_s* rq,process_t* p){node_t* n=(node_t*)malloc(sizeof(node_t));n->proc=p;n->next=NULL;node_t* c=&rq->sentinel;while(c->next){if(p->priority<c->next->proc->priority)break;c=c->next;}n->next=c->next;c->next=n;rq->size++;}
void push_sjf2(struct gQ_s* rq,process_t* p){node_t* n=(node_t*)malloc(sizeof(node_t));n->proc=p;n->next=NULL;node_t* c=&rq->sentinel;while(c->next){if(p->burst_time<c->next->proc->burst_time)break;c=c->next;}n->next=c->next;c->next=n;rq->size++;}
void push_hpc2(struct gQ_s* rq,process_t* p){node_t* n=(node_t*)malloc(sizeof(node_t));n->proc=p;n->next=rq->sentinel.next;rq->sentinel.next=n;rq->size++;}
void ready_queue_push2(struct gQ_s* rq,process_t* proc){
  pthread_mutex_lock(&rq->m);
  if(!proc){node_t* s=(node_t*)malloc(sizeof(node_t));s->proc=NULL;s->next=NULL;node_t* c=&rq->sentinel;while(c->next)c=c->next;c->next=s;rq->size++;pthread_cond_broadcast(&rq->c);pthread_mutex_unlock(&rq->m);return;}
  switch(rq->alg){
    case ALG_FIFO:case ALG_RR:case ALG_BFS:case ALG_CFS:case ALG_CFS_SRTF:case ALG_STRF:case ALG_HRRN:case ALG_HRRN_RT:case ALG_MLFQ:push_fifo2(rq,proc);break;
    case ALG_PRIORITY:push_priority2(rq,proc);break;case ALG_SJF:push_sjf2(rq,proc);break;case ALG_HPC:push_hpc2(rq,proc);break;default:push_fifo2(rq,proc);break;
  }
  pthread_cond_broadcast(&rq->c);pthread_mutex_unlock(&rq->m);
}
process_t* ready_queue_pop2(struct gQ_s* rq){
  pthread_mutex_lock(&rq->m);while(1){if(rq->size>0){process_t* p=pop_head2(rq);pthread_mutex_unlock(&rq->m);return p;}pthread_cond_wait(&rq->c,&rq->m);}pthread_mutex_unlock(&rq->m);return NULL;
}
