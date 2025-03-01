#include "ready_queue.h"
#include "../lib/log.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------
   Internal comparator for sorted insertion
   ------------------------------------------------------ */
typedef int (*proc_cmp_fn)(const process_t* A, const process_t* B);

static int prio_asc_cmp(const process_t* A, const process_t* B){
    return (A->priority - B->priority);
}
static int burst_asc_cmp(const process_t* A, const process_t* B){
    if(A->burst_time < B->burst_time) return -1;
    if(A->burst_time > B->burst_time) return 1;
    return 0;
}

/* Simple ascending sort insertion if cmp != NULL,
   or tail insertion (FIFO) if cmp == NULL. */
static void rq_insert_sorted(ready_queue_t* rq, process_t* p, proc_cmp_fn cmp){
    rq_node_t* n = (rq_node_t*)malloc(sizeof(rq_node_t));
    n->proc = p;
    n->next = NULL;

    if(!rq->head){
        rq->head = n;
        return;
    }
    if(!cmp){
        /* Insert at tail => FIFO */
        rq_node_t* c = rq->head;
        while(c->next) c = c->next;
        c->next = n;
        return;
    }
    /* Else ascending sorted insertion. */
    rq_node_t* c  = rq->head;
    rq_node_t* px = NULL;
    while(c){
        if(cmp(p, c->proc) < 0){
            if(px){
                px->next = n;
            } else {
                rq->head = n;
            }
            n->next = c;
            return;
        }
        px = c;
        c = c->next;
    }
    px->next = n;
}

void rq_init(ready_queue_t* rq, scheduler_alg_t alg){
    memset(rq, 0, sizeof(*rq));
    pthread_mutex_init(&rq->lock, NULL);
    pthread_cond_init(&rq->cond, NULL);
    rq->alg = alg;
    rq->wfq_virtual_time = 0.0;
}

void rq_destroy(ready_queue_t* rq){
    if(!rq) return;
    rq_node_t* c = rq->head;
    while(c){
        rq_node_t* nxt = c->next;
        free(c);
        c = nxt;
    }
    pthread_mutex_destroy(&rq->lock);
    pthread_cond_destroy(&rq->cond);
    memset(rq, 0, sizeof(*rq));
}

void rq_push(ready_queue_t* rq, process_t* p){
    pthread_mutex_lock(&rq->lock);

    if(!p){
        /* termination marker => push front always */
        rq_node_t* n = (rq_node_t*)malloc(sizeof(rq_node_t));
        n->proc = NULL;
        n->next = rq->head;
        rq->head = n;
        rq->size++;
        pthread_cond_broadcast(&rq->cond);
        pthread_mutex_unlock(&rq->lock);
        return;
    }

    switch(rq->alg){
    case ALG_PRIORITY:
        /* Non-preemptive priority => ascending prio. */
        rq_insert_sorted(rq, p, prio_asc_cmp);
        break;
    case ALG_PRIO_PREEMPT:
        /* Preemptive => also ascending prio. */
        rq_insert_sorted(rq, p, prio_asc_cmp);
        break;
    case ALG_SJF:
        /* Insert by burst ascending. */
        rq_insert_sorted(rq, p, burst_asc_cmp);
        break;
    case ALG_HPC:
        /* HPC example => push front. */
    {
        rq_node_t* n = (rq_node_t*)malloc(sizeof(rq_node_t));
        n->proc = p;
        n->next = rq->head;
        rq->head = n;
    }
        break;
    default:
        /* default => FIFO for RR, BFS, MLFQ, WFQ, etc. */
        rq_insert_sorted(rq, p, NULL);
        break;
    }

    rq->size++;
    pthread_cond_broadcast(&rq->cond);
    pthread_mutex_unlock(&rq->lock);
}

process_t* rq_pop(ready_queue_t* rq, bool* got_term){
    *got_term = false;
    pthread_mutex_lock(&rq->lock);

    /* Wait until something is in the queue. */
    while(rq->size == 0){
        pthread_cond_wait(&rq->cond, &rq->lock);
    }

    if(rq->alg == ALG_WFQ){
        /* Weighted Fair Queueing => pick process with earliest
           "finish_time" = (current wfq_virtual_time + remaining / weight).
           But we no longer do a big jump of 'wfq_virtual_time += entire_burst'
           here. Instead, that is done partially in run_slice. */
        rq_node_t* prev = NULL;
        rq_node_t* best_prev = NULL;
        rq_node_t* best_node = NULL;
        double best_val = 1e15;

        rq_node_t* cur = rq->head;
        while(cur){
            if(!cur->proc){
                /* termination marker => pick that immediately. */
                best_node = cur;
                break;
            }
            double finish_time =
                rq->wfq_virtual_time + ((double)cur->proc->remaining_time / cur->proc->weight);
            if(finish_time < best_val){
                best_val = finish_time;
                best_node = cur;
                best_prev = prev;
            }
            prev = cur;
            cur = cur->next;
        }
        if(!best_node){
            pthread_mutex_unlock(&rq->lock);
            return NULL;
        }
        if(!best_node->proc){
            *got_term = true;
            /* remove best_node from list */
            if(best_node == rq->head){
                rq->head = best_node->next;
            } else if(best_prev){
                best_prev->next = best_node->next;
            }
            free(best_node);
            rq->size--;
            pthread_mutex_unlock(&rq->lock);
            return NULL;
        }
        process_t* ret = best_node->proc;
        if(best_node == rq->head){
            rq->head = best_node->next;
        } else if(best_prev){
            best_prev->next = best_node->next;
        }
        free(best_node);
        rq->size--;
        pthread_mutex_unlock(&rq->lock);
        return ret;
    }
    else {
        /* Non-WFQ => pop head. */
        rq_node_t* h = rq->head;
        rq->head = h->next;
        rq->size--;

        process_t* ret = h->proc;
        if(!ret){
            *got_term = true;
        }
        free(h);
        pthread_mutex_unlock(&rq->lock);
        return ret;
    }
}

/* Attempt to see if new highest-priority arrival can preempt. */
bool try_preempt_if_needed(ready_queue_t* rq, process_t* p){
    if(rq->alg != ALG_PRIO_PREEMPT || !p) return false;

    pthread_mutex_lock(&rq->lock);
    if(!rq->head || !rq->head->proc){
        pthread_mutex_unlock(&rq->lock);
        return false;
    }
    process_t* front = rq->head->proc;
    /* If the *front* of the queue has strictly lower priority number
       => that means it's higher priority (since smaller prio # is "higher"). */
    if(front->priority < p->priority){
        /* We do a forced preempt => put p back, pop the new highest prio. */
        p->was_preempted = true;
        rq_insert_sorted(rq, p, prio_asc_cmp);
        rq->size++;
        pthread_mutex_unlock(&rq->lock);
        return true;
    }
    pthread_mutex_unlock(&rq->lock);
    return false;
}
