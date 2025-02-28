#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include "concurrency.h"
#include "ephemeral.h"
#include "log.h"

/* ---------------------------
   PROCESS INIT
   --------------------------- */
void init_process(process_t* p, unsigned long burst, int prio, unsigned long arrival, double weight){
    if(!p){
        log_error("init_process => Null pointer for process_t");
        return;
    }
    if(burst == 0){
        log_warn("init_process => burst=0. Process will complete instantly.");
    }
    if(prio < 0){
        log_warn("init_process => negative priority => continuing but check logic.");
    }
    if(weight <= 0.0){
        log_warn("init_process => nonpositive weight => forcing weight=1.0");
        weight = 1.0;
    }

    p->id             = 0;
    p->burst_time     = burst;
    p->priority       = prio;
    p->arrival_time   = arrival;
    p->remaining_time = burst;
    p->first_response = 0;
    p->end_time       = 0;
    p->responded      = false;
    p->weight         = weight;
    p->hpc_affinity   = -1;
    p->mlfq_level     = 0;
    p->was_preempted  = false;
}

/* ---------------------------
   READY QUEUE STRUCTURES
   --------------------------- */
typedef struct rq_node_s {
    process_t*        proc;
    struct rq_node_s* next;
} rq_node_t;

typedef struct {
    rq_node_t*       head;
    int              size;
    pthread_mutex_t  lock;
    pthread_cond_t   cond;
    scheduler_alg_t  alg;
    double           wfq_virtual_time; /* For Weighted Fair Queueing. */
} ready_queue_t;

/* ---------------------------
   HELPER: INIT / DESTROY RQ
   --------------------------- */
static void rq_init(ready_queue_t* rq, scheduler_alg_t alg){
    memset(rq, 0, sizeof(*rq));
    pthread_mutex_init(&rq->lock, NULL);
    pthread_cond_init(&rq->cond, NULL);
    rq->alg = alg;
    rq->wfq_virtual_time = 0.0;
}

static void rq_destroy(ready_queue_t* rq){
    if(!rq) return;
    rq_node_t* c = rq->head;
    while(c){
        rq_node_t* nx = c->next;
        free(c);
        c = nx;
    }
    pthread_mutex_destroy(&rq->lock);
    pthread_cond_destroy(&rq->cond);
    memset(rq, 0, sizeof(*rq));
}

/* Comparators for sorted insertion */
typedef int (*proc_cmp_fn)(const process_t* A, const process_t* B);

static int prio_asc_cmp(const process_t* A, const process_t* B){
    return (A->priority - B->priority); /* smaller => front */
}
static int burst_asc_cmp(const process_t* A, const process_t* B){
    if(A->burst_time < B->burst_time) return -1;
    if(A->burst_time > B->burst_time) return 1;
    return 0;
}

static void rq_insert_sorted(ready_queue_t* rq, process_t* p, proc_cmp_fn cmp){
    rq_node_t* n = (rq_node_t*)malloc(sizeof(rq_node_t));
    n->proc = p;
    n->next = NULL;

    if(!rq->head){
        rq->head = n;
        return;
    }
    if(!cmp){
        /* Insert at tail. */
        rq_node_t* c = rq->head;
        while(c->next) c = c->next;
        c->next = n;
        return;
    }
    /* Ascending sort insertion. */
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

/*
   Main push function
*/
static void rq_push(ready_queue_t* rq, process_t* p){
    rq_node_t* n = NULL;

    pthread_mutex_lock(&rq->lock);

    if(!p){
        /* termination marker => push front always. */
        n = (rq_node_t*)malloc(sizeof(rq_node_t));
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
            /* Non-preemptive priority => ascending order by prio. */
            rq_insert_sorted(rq, p, prio_asc_cmp);
            break;
        case ALG_SJF:
            /* Insert by burst ascending. */
            rq_insert_sorted(rq, p, burst_asc_cmp);
            break;
        case ALG_HPC:
            /* HPC example => push front. */
            n = (rq_node_t*)malloc(sizeof(rq_node_t));
            n->proc = p;
            n->next = rq->head;
            rq->head = n;
            break;
        case ALG_PRIO_PREEMPT:
            /* Keep ascending order for preemptive priority. */
            rq_insert_sorted(rq, p, prio_asc_cmp);
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

/*
   Weighted Fair Queueing pop => earliest finish time
   otherwise => pop head
*/
static process_t* rq_pop(ready_queue_t* rq, bool* got_termination_marker){
    *got_termination_marker = false;
    pthread_mutex_lock(&rq->lock);

    while(rq->size == 0){
        pthread_cond_wait(&rq->cond, &rq->lock);
    }

    process_t* ret = NULL;

    if(rq->alg == ALG_WFQ){
        rq_node_t* prev=NULL,* best_prev=NULL,* best_node=NULL;
        double best_val = 1e15;
        rq_node_t* cur = rq->head;
        while(cur){
            if(!cur->proc){
                /* termination marker => pick that immediately. */
                best_node = cur;
                break;
            }
            double finish = rq->wfq_virtual_time
                + ((double)cur->proc->remaining_time / cur->proc->weight);
            if(finish < best_val){
                best_val = finish;
                best_node = cur;
                best_prev = prev;
            }
            prev = cur;
            cur  = cur->next;
        }
        if(!best_node){
            pthread_mutex_unlock(&rq->lock);
            return NULL;
        }
        if(!best_node->proc){
            /* termination marker => end thread. */
            *got_termination_marker = true;
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
        ret = best_node->proc;
        if(best_node == rq->head){
            rq->head = best_node->next;
        } else if(best_prev){
            best_prev->next = best_node->next;
        }
        free(best_node);
        rq->size--;

        rq->wfq_virtual_time += (double)ret->remaining_time / ret->weight;
        pthread_mutex_unlock(&rq->lock);
        return ret;
    }
    else {
        /* Non-WFQ => pop head. */
        rq_node_t* h = rq->head;
        rq->head = h->next;
        rq->size--;
        ret = h->proc;
        if(!ret){
            *got_termination_marker = true;
        }
        free(h);
        pthread_mutex_unlock(&rq->lock);
        return ret;
    }
}

/*
   For preemptive priority: check if we need to forcibly preempt.
*/
static bool try_preempt_if_needed(ready_queue_t* rq, process_t* p){
    if(rq->alg != ALG_PRIO_PREEMPT || !p) return false;

    pthread_mutex_lock(&rq->lock);

    if(!rq->head || !rq->head->proc){
        pthread_mutex_unlock(&rq->lock);
        return false;
    }
    process_t* front = rq->head->proc;
    if(front->priority < p->priority){
        /* Preempt => put p back, pop the new highest. */
        p->was_preempted = true;
        rq_insert_sorted(rq, p, prio_asc_cmp);
        rq->size++;
        pthread_mutex_unlock(&rq->lock);
        return true;
    }
    pthread_mutex_unlock(&rq->lock);
    return false;
}

/* ---------------------------
   CONTAINER QUEUES
   --------------------------- */
typedef struct {
    ready_queue_t main_rq;
    ready_queue_t hpc_rq;
} container_queues_t;

/* ---------------------------
   TIMELINE
   --------------------------- */
static void record_timeline(container_t* c, int core_id, int proc_id,
                            unsigned long start_ms, unsigned long slice,
                            bool preempted)
{
    pthread_mutex_lock(&c->timeline_lock);
    if(c->timeline_count+1 >= c->timeline_cap){
        c->timeline_cap += 100;
        c->timeline = realloc(c->timeline, c->timeline_cap*sizeof(*c->timeline));
        if(!c->timeline){
            log_error("record_timeline => out of memory allocating timeline!");
            pthread_mutex_unlock(&c->timeline_lock);
            return;
        }
    }
    c->timeline[c->timeline_count].core_id         = core_id;
    c->timeline[c->timeline_count].proc_id         = proc_id;
    c->timeline[c->timeline_count].start_ms        = start_ms;
    c->timeline[c->timeline_count].length_ms       = slice;
    c->timeline[c->timeline_count].preempted_slice = preempted;
    c->timeline_count++;
    pthread_mutex_unlock(&c->timeline_lock);
}

/* CPU-bound simulation, scaled for demonstration. */
static void do_cpu_work(unsigned long ms){
    usleep((useconds_t)(ms * 3000U));
}

/*
   GET QUANTUM (PER ALGORITHM)
*/
static unsigned long get_quantum(scheduler_alg_t alg, process_t* p){
    if(!p) return 0;
    switch(alg){
        case ALG_RR:   return 2;
        case ALG_BFS:  return 4;
        case ALG_WFQ:  return 3;
        case ALG_MLFQ: {
            /* Example MLFQ: base 2 + 2 per level. */
            return (2 + p->mlfq_level*2);
        }
        case ALG_PRIO_PREEMPT:
            return 2;
        default:
            return 2; /* fallback for FIFO, SJF, PRIORITY, HPC. */
    }
}

/*
   SCHEDULING SLICE ROUTINE
*/
static void run_slice(container_t* c, process_t* p,
                      scheduler_alg_t alg, int core_id,
                      container_queues_t* qs,
                      unsigned long* used_ms)
{
    if(!p || p->remaining_time == 0){
        *used_ms = 0;
        return;
    }

    unsigned long quantum = get_quantum(alg, p);

    pthread_mutex_lock(&c->finish_lock);
    unsigned long start_ms = c->sim_time;
    if(!p->responded){
        p->responded = true;
        p->first_response = start_ms;
    }
    pthread_mutex_unlock(&c->finish_lock);

    unsigned long slice_used = 0;
    unsigned long slice_remaining = quantum;
    bool preempted_flag = false;

    while(slice_remaining > 0 && !c->time_exhausted && p->remaining_time > 0){
        unsigned long step = (slice_remaining < p->remaining_time)
                             ? slice_remaining : p->remaining_time;

        if(step > 0){
            do_cpu_work(step);
        }

        pthread_mutex_lock(&c->finish_lock);

        p->remaining_time -= step;
        c->accumulated_cpu += step;
        c->sim_time        += step;

        slice_used += step;
        slice_remaining -= step;

        if(p->remaining_time == 0){
            p->end_time = p->first_response + p->burst_time;
            c->remaining_count -= 1;
            if(c->remaining_count <= 0){
                c->time_exhausted = true;
            }
        }
        if(c->accumulated_cpu >= c->max_cpu_time_ms){
            c->time_exhausted = true;
        }
        pthread_mutex_unlock(&c->finish_lock);

        /* For preemptive priority, check arrivals or higher-priority arrivals. */
        if(alg == ALG_PRIO_PREEMPT){
            bool got_preempted = try_preempt_if_needed(&qs->main_rq, p);
            if(got_preempted){
                preempted_flag = true;
                break;
            }
        }

        if(c->time_exhausted){
            break;
        }
    }

    if(alg == ALG_MLFQ && p->remaining_time>0 && slice_used == quantum){
        /* MLFQ => demote if used entire quantum. */
        p->mlfq_level++;
    }

    *used_ms = slice_used;
    record_timeline(c, core_id, p->id, start_ms, slice_used, preempted_flag);
}

/* STOP + ARRIVAL CHECKS */
static bool is_time_exhausted(container_t* c){
    pthread_mutex_lock(&c->finish_lock);
    bool r = c->time_exhausted ||
             (c->accumulated_cpu >= c->max_cpu_time_ms);
    pthread_mutex_unlock(&c->finish_lock);
    return r;
}

static void force_stop(container_t* c, ready_queue_t* rq_main, ready_queue_t* rq_hpc){
    for(int i=0;i<c->nb_cores;i++){
        rq_push(rq_main, NULL);
    }
    for(int i=0;i<c->nb_hpc_threads;i++){
        rq_push(rq_hpc, NULL);
    }
}

static void check_main_arrivals(container_t* c, ready_queue_t* rq){
    pthread_mutex_lock(&c->finish_lock);
    unsigned long now = c->sim_time;
    pthread_mutex_unlock(&c->finish_lock);

    for(int i=0;i<c->main_count;i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0){
            if(p->arrival_time <= now){
                rq_push(rq, p);
                p->arrival_time = 0;
            }
        }
    }
}

static void check_hpc_arrivals(container_t* c, ready_queue_t* rq){
    pthread_mutex_lock(&c->finish_lock);
    unsigned long now = c->sim_time;
    pthread_mutex_unlock(&c->finish_lock);

    for(int i=0;i<c->hpc_count;i++){
        process_t* p = &c->hpc_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0){
            if(p->arrival_time <= now){
                rq_push(rq, p);
                p->arrival_time = 0;
            }
        }
    }
}

/* PER-CORE THREAD */
typedef struct {
    container_t*       container;
    container_queues_t*qs;
    int                core_id;
} core_pack_t;

static void* main_core_thread(void* arg){
    core_pack_t* pack = (core_pack_t*)arg;
    container_t* c    = pack->container;
    container_queues_t* qs = pack->qs;
    int core_id       = pack->core_id;
    free(pack);

    while(!is_time_exhausted(c)){
        bool term_marker=false;
        process_t* p = rq_pop(&qs->main_rq, &term_marker);
        if(term_marker || !p){
            break;
        }
        unsigned long used=0;
        run_slice(c, p, c->main_alg, core_id, qs, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            rq_push(&qs->main_rq, p);
        }

        check_main_arrivals(c, &qs->main_rq);
        check_hpc_arrivals(c, &qs->hpc_rq);

        if(is_time_exhausted(c)){
            force_stop(c, &qs->main_rq, &qs->hpc_rq);
            break;
        }
    }
    return NULL;
}

/* HPC THREAD */
static void* hpc_thread(void* arg){
    core_pack_t* pack = (core_pack_t*)arg;
    container_t* c    = pack->container;
    container_queues_t* qs = pack->qs;
    int hpc_id = pack->core_id; /* negative ID for timeline. */
    free(pack);

    while(!is_time_exhausted(c)){
        bool term_marker=false;
        process_t* p = rq_pop(&qs->hpc_rq, &term_marker);
        if(term_marker || !p){
            break;
        }

        unsigned long used=0;
        run_slice(c, p, c->hpc_alg, hpc_id, qs, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            rq_push(&qs->hpc_rq, p);
        }

        /* HPC steal from main if allowed + HPC is empty? */
        if(c->allow_hpc_steal){
            pthread_mutex_lock(&qs->hpc_rq.lock);
            int hpc_size = qs->hpc_rq.size;
            pthread_mutex_unlock(&qs->hpc_rq.lock);
            if(hpc_size == 0){
                bool dummy=false;
                process_t* stolen = rq_pop(&qs->main_rq, &dummy);
                if(stolen && !dummy){
                    run_slice(c, stolen, c->main_alg, hpc_id, qs, &used);
                    if(!is_time_exhausted(c) && stolen->remaining_time>0){
                        rq_push(&qs->main_rq, stolen);
                    }
                }
            }
        }

        check_main_arrivals(c, &qs->main_rq);
        check_hpc_arrivals(c, &qs->hpc_rq);

        if(is_time_exhausted(c)){
            force_stop(c, &qs->main_rq, &qs->hpc_rq);
            break;
        }
    }
    return NULL;
}

/* PRINT TIMELINE */
static int compare_timeline(const void* a, const void* b){
    const typeof(((container_t*)0)->timeline[0]) *A = (const void*)a;
    const typeof(((container_t*)0)->timeline[0]) *B = (const void*)b;
    if(A->core_id < B->core_id) return -1;
    if(A->core_id > B->core_id) return 1;
    if(A->start_ms < B->start_ms) return -1;
    if(A->start_ms > B->start_ms) return 1;
    return 0;
}

static void print_container_timeline(container_t* c){
    if(c->timeline_count==0){
        printf("\n\033[1m\033[33mNo timeline for container.\n\033[0m");
        return;
    }
    qsort(c->timeline, c->timeline_count,
          sizeof(c->timeline[0]), compare_timeline);

    printf("\033[1m\033[36m\n--- Container Timeline ---\n\033[0m");
    int current_core = 999999999;
    for(int i=0;i<c->timeline_count;i++){
        int core_id = c->timeline[i].core_id;
        if(core_id != current_core){
            if(core_id >= 0){
                printf("\033[1m\033[32m\nMain Core %d:\n\033[0m", core_id);
            } else {
                int hpc_idx = (-1 - core_id);
                printf("\033[1m\033[35m\nHPC Thread %d:\n\033[0m", hpc_idx);
            }
            current_core = core_id;
        }
        unsigned long st = c->timeline[i].start_ms;
        unsigned long ln = c->timeline[i].length_ms;
        bool pre = c->timeline[i].preempted_slice;
        if(pre){
            printf("  T[%lu..%lu] => P%d \033[1m\033[33m[PREEMPT]\033[0m\n",
                   st, st+ln, c->timeline[i].proc_id);
        } else {
            printf("  T[%lu..%lu] => P%d\n", st, st+ln, c->timeline[i].proc_id);
        }
    }
}

/* CONTAINER RUN + INIT */
static void* container_run(void* arg){
    container_t* c = (container_t*)arg;
    if(!c){
        log_error("container_run => null container pointer?");
        return NULL;
    }

    c->ephemeral_path = ephemeral_create_container();
    if(!c->ephemeral_path){
        log_error("container_run => ephemeral creation failed. Will run anyway.");
    }

    /* Give IDs. HPC offset by 1000. */
    for(int i=0;i<c->main_count;i++){
        c->main_procs[i].id = i;
    }
    for(int i=0;i<c->hpc_count;i++){
        c->hpc_procs[i].id = 1000 + i;
    }

    /* Create local queues */
    container_queues_t qs;
    rq_init(&qs.main_rq, c->main_alg);
    rq_init(&qs.hpc_rq,  c->hpc_alg);

    /* Push immediate arrivals (arrival_time=0). */
    for(int i=0;i<c->main_count;i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time==0){
            rq_push(&qs.main_rq, p);
        }
    }
    for(int i=0;i<c->hpc_count;i++){
        process_t* p = &c->hpc_procs[i];
        if(p->remaining_time>0 && p->arrival_time==0){
            rq_push(&qs.hpc_rq, p);
        }
    }

    /* Start main core threads. */
    pthread_t* main_threads = (pthread_t*)calloc(c->nb_cores, sizeof(pthread_t));
    if(!main_threads){
        log_error("container_run => cannot allocate main_threads");
        return NULL;
    }
    for(int i=0;i<c->nb_cores;i++){
        core_pack_t* pack = (core_pack_t*)malloc(sizeof(core_pack_t));
        if(!pack){
            log_error("container_run => cannot allocate core_pack_t");
            continue;
        }
        pack->container = c;
        pack->qs        = &qs;
        pack->core_id   = i;
        pthread_create(&main_threads[i], NULL, main_core_thread, pack);
    }

    /* Start HPC threads. */
    pthread_t* hpc_threads = NULL;
    if(c->nb_hpc_threads>0){
        hpc_threads = (pthread_t*)calloc(c->nb_hpc_threads, sizeof(pthread_t));
        if(!hpc_threads){
            log_error("container_run => cannot allocate hpc_threads");
        } else {
            for(int i=0;i<c->nb_hpc_threads;i++){
                core_pack_t* pack = (core_pack_t*)malloc(sizeof(core_pack_t));
                if(!pack){
                    log_error("container_run => cannot allocate HPC core_pack_t");
                    continue;
                }
                pack->container = c;
                pack->qs        = &qs;
                pack->core_id   = -1 - i;
                pthread_create(&hpc_threads[i], NULL, hpc_thread, pack);
            }
        }
    }

    /* Join main threads. */
    for(int i=0;i<c->nb_cores;i++){
        pthread_join(main_threads[i], NULL);
    }
    free(main_threads);

    /* Join HPC threads. */
    if(c->nb_hpc_threads>0 && hpc_threads){
        for(int i=0;i<c->nb_hpc_threads;i++){
            pthread_join(hpc_threads[i], NULL);
        }
        free(hpc_threads);
    }

    /* Destroy queues */
    rq_destroy(&qs.main_rq);
    rq_destroy(&qs.hpc_rq);

    /* ephemeral remove */
    if(c->ephemeral_path){
        ephemeral_remove_container(c->ephemeral_path);
        free(c->ephemeral_path);
        c->ephemeral_path = NULL;
    }

    /* Print timeline */
    print_container_timeline(c);

    /* Clean up timeline array. */
    if(c->timeline){
        free(c->timeline);
        c->timeline = NULL;
    }
    pthread_mutex_destroy(&c->timeline_lock);
    pthread_mutex_destroy(&c->finish_lock);

    return NULL;
}

void container_init(container_t* c,
                    int nb_cores,
                    int nb_hpc_threads,
                    scheduler_alg_t main_alg,
                    scheduler_alg_t hpc_alg,
                    process_t* main_list,
                    int main_count,
                    process_t* hpc_list,
                    int hpc_count,
                    unsigned long max_cpu_ms)
{
    if(!c){
        log_error("container_init => container pointer is NULL");
        return;
    }
    memset(c, 0, sizeof(*c));

    if(nb_cores < 0 || nb_hpc_threads < 0){
        log_warn("container_init with negative core/hpc => forcing 0");
        if(nb_cores < 0)        nb_cores = 0;
        if(nb_hpc_threads < 0)  nb_hpc_threads = 0;
    }
    if(max_cpu_ms == 0){
        log_warn("container_init => max_cpu_ms=0 => forcing 100");
        max_cpu_ms = 100;
    }

    c->nb_cores        = nb_cores;
    c->nb_hpc_threads  = nb_hpc_threads;
    c->main_alg        = main_alg;
    c->hpc_alg         = hpc_alg;
    c->main_procs      = main_list;
    c->main_count      = main_count;
    c->hpc_procs       = hpc_list;
    c->hpc_count       = hpc_count;
    c->max_cpu_time_ms = max_cpu_ms;
    c->remaining_count = main_count + hpc_count;

    pthread_mutex_init(&c->finish_lock, NULL);
    pthread_mutex_init(&c->timeline_lock, NULL);

    c->timeline        = NULL;
    c->timeline_count  = 0;
    c->timeline_cap    = 0;
    c->time_exhausted  = false;
    c->accumulated_cpu = 0;
    c->sim_time        = 0;

    /* If we have 0 main cores but still have main processes,
       let HPC threads steal from main automatically to avoid deadlock. */
    if(nb_cores == 0 && main_count > 0){
        log_info("container_init => no main cores but main processes exist => enabling HPC steal");
        c->allow_hpc_steal = true;
    } else {
        c->allow_hpc_steal = false;
    }
}

void orchestrator_run(container_t* arr, int count){
    pthread_t* tids = (pthread_t*)calloc(count, sizeof(pthread_t));
    if(!tids){
        log_error("orchestrator_run => cannot allocate thread array");
        return;
    }
    for(int i=0;i<count;i++){
        pthread_create(&tids[i], NULL, container_run, &arr[i]);
    }
    for(int i=0;i<count;i++){
        pthread_join(tids[i], NULL);
    }
    free(tids);
}
