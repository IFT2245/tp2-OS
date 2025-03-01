#include "worker.h"
#include "ready_queue.h"
#include "scheduler.h"
#include "../lib/library.h"
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
   (1) INTERNAL UTILITY: Check if container has exceeded max time
   ------------------------------------------------------------------ */
static bool is_time_exhausted(container_t* c){
    pthread_mutex_lock(&c->finish_lock);
    bool r = (c->time_exhausted || (c->accumulated_cpu >= c->max_cpu_time_ms));
    pthread_mutex_unlock(&c->finish_lock);
    return r;
}

/* ------------------------------------------------------------------
   (2) INTERNAL UTILITY: Forcibly push a termination marker into
       both main & HPC RQs so all threads can break out.
   ------------------------------------------------------------------ */
static void force_stop(const container_t* c, ready_queue_t* rq_main, ready_queue_t* rq_hpc){
    for(int i=0; i<c->nb_cores; i++){
        rq_push(rq_main, NULL);
    }
    for(int i=0; i<c->nb_hpc_threads; i++){
        rq_push(rq_hpc, NULL);
    }
}

/* ------------------------------------------------------------------
   (3) INTERNAL UTILITY: Check arrivals for any processes whose
       arrival_time <= current sim_time, push them, and set
       arrival_time=0 so we don't re-push.
   ------------------------------------------------------------------ */
static void check_main_arrivals(container_t* c, ready_queue_t* rq){
    pthread_mutex_lock(&c->finish_lock);
    unsigned long now = c->sim_time;
    pthread_mutex_unlock(&c->finish_lock);

    for(int i=0; i<c->main_count; i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time <= now){
            fprintf(stderr, "\033[94m[MAIN ARRIVE] P%d arrives at t=%lu => push mainRQ\033[0m\n",
                    p->id, now);
            p->arrival_time = 0;  // mark arrived
            rq_push(rq, p);
        }
    }
}

static void check_hpc_arrivals(container_t* c, ready_queue_t* rq){
    pthread_mutex_lock(&c->finish_lock);
    unsigned long now = c->sim_time;
    pthread_mutex_unlock(&c->finish_lock);

    for(int i=0; i<c->hpc_count; i++){
        process_t* p = &c->hpc_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time <= now){
            fprintf(stderr, "\033[95m[HPC ARRIVE]  P%d arrives at t=%lu => push hpcRQ\033[0m\n",
                    p->id, now);
            p->arrival_time = 0; // mark arrived
            rq_push(rq, p);
        }
    }
}

/* ------------------------------------------------------------------
   (4) INTERNAL UTILITY: "DISCRETE-EVENT" ADVANCE WHEN BOTH QUEUES
       ARE EMPTY AND NO CORE IS BUSY (active_cores == 0).
       If no future arrivals => push termination marker.
       Otherwise, jump to the earliest arrival time.
   ------------------------------------------------------------------ */
static void maybe_advance_time_if_idle(container_t* c,
                                       ready_queue_t* main_rq,
                                       ready_queue_t* hpc_rq){
    pthread_mutex_lock(&c->finish_lock);

    // If either queue is nonempty => no jump
    if(main_rq->size > 0) {
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }
    if(!c->allow_hpc_steal && hpc_rq->size > 0) {
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }

    // If more than 0 cores are busy => no jump
    if(c->active_cores > 0) {
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }

    // Find earliest future arrival
    unsigned long earliest = (unsigned long)-1;

    for(int i=0; i<c->main_count; i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time < earliest){
            earliest = p->arrival_time;
        }
    }
    for(int i=0; i<c->hpc_count; i++){
        process_t* p = &c->hpc_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time < earliest){
            earliest = p->arrival_time;
        }
    }

    if(earliest == (unsigned long)-1) {
        // No future arrivals => push termination
        rq_push(main_rq, NULL);
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }

    // Jump sim_time to earliest
    c->sim_time = earliest;
    pthread_mutex_unlock(&c->finish_lock);

    // Now that time jumped, check arrivals right away
    check_main_arrivals(c, main_rq);
    check_hpc_arrivals(c, hpc_rq);
}

/* ------------------------------------------------------------------
   (5) ACTUAL TIMESLICE RUN, BROKEN INTO 1ms CHUNKS:
   - This ensures BFS and RR can see new arrivals that happen mid-slice.
   - Also allows preemptive check each millisecond.
   ------------------------------------------------------------------ */
static void run_slice(container_t* c,
                      ready_queue_t* main_rq,
                      ready_queue_t* hpc_rq,
                      process_t* p,
                      scheduler_alg_t alg,
                      int core_id,
                      unsigned long* used_ms)
{
    *used_ms = 0;
    if(!p || p->remaining_time == 0) return;

    unsigned long quantum = get_quantum(alg, p);

    pthread_mutex_lock(&c->finish_lock);
    c->active_cores++;
    unsigned long start_ms = c->sim_time;
    if(!p->responded){
        p->responded      = true;
        p->first_response = start_ms;
    }
    pthread_mutex_unlock(&c->finish_lock);

    bool preempted_flag = false;
    unsigned long slice_used = 0;

    while(slice_used < quantum && !c->time_exhausted && p->remaining_time > 0) {
        // We'll do CPU work of exactly 1ms:
        do_cpu_work(1, core_id, p->id);

        // Update container sim_time, process remaining_time, etc.
        pthread_mutex_lock(&c->finish_lock);
        p->remaining_time--;
        c->accumulated_cpu++;
        c->sim_time++;
        slice_used++;

        if(p->remaining_time == 0){
            p->end_time = p->first_response + p->burst_time;
            c->remaining_count--;
            if(c->remaining_count <= 0){
                c->time_exhausted = true;
            }
        }
        if(c->accumulated_cpu >= c->max_cpu_time_ms){
            c->time_exhausted = true;
        }
        pthread_mutex_unlock(&c->finish_lock);

        // Weighted Fair Queueing => update each ms:
        if(alg == ALG_WFQ){
            pthread_mutex_lock(&main_rq->lock);
            main_rq->wfq_virtual_time += (1.0 / p->weight);
            pthread_mutex_unlock(&main_rq->lock);
        }

        // Preemptive priority => check each ms
        if(alg == ALG_PRIO_PREEMPT){
            bool got_preempted = try_preempt_if_needed(main_rq, p);
            if(got_preempted) {
                preempted_flag = true;
                break;
            }
        }

        // Also check arrivals each ms so BFS, RR, etc. see arrivals:
        check_main_arrivals(c, main_rq);
        check_hpc_arrivals(c, hpc_rq);

        // If time is exhausted => stop
        if(is_time_exhausted(c)) break;
    }

    // MLFQ: if entire quantum used & not finished => next queue level
    if(alg == ALG_MLFQ && p->remaining_time>0 && slice_used == quantum){
        p->mlfq_level++;
    }

    *used_ms = slice_used;
    record_timeline(c, core_id, p->id, start_ms, slice_used, preempted_flag);

    if(preempted_flag){
        fprintf(stderr, "\033[33m[CORE %d] PREEMPTED process P%d after %lu ms!\033[0m\n",
                core_id, p->id, slice_used);
    }

    pthread_mutex_lock(&c->finish_lock);
    c->active_cores--;
    pthread_mutex_unlock(&c->finish_lock);
}

/* ------------------------------------------------------------------
   (6) MAIN CORE THREAD
   - Repeatedly pop from main RQ and do run_slice.
   - If the queue is empty, we might do maybe_advance_time_if_idle.
   ------------------------------------------------------------------ */
void* main_core_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c         = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int core_id            = pack->core_id;
    free(pack);

    set_core_id_for_this_thread(core_id);

    while(!is_time_exhausted(c)){
        block_preempt_signal();

        sigjmp_buf env;
        int ret = sigsetjmp(env, 1);
        register_jmpbuf_for_core(core_id, env);

        if(ret != 0){
            fprintf(stderr, "\033[31m[CORE %d] *** IMMEDIATE PREEMPT => jumped back ***\033[0m\n",
                    core_id);
        }

        maybe_advance_time_if_idle(c, main_rq, hpc_rq);

        bool term_marker=false;
        process_t* p = rq_pop(main_rq, &term_marker);
        if(term_marker || !p){
            fprintf(stderr, "\033[32m[CORE %d] Termination => done.\033[0m\n", core_id);
            break;
        }

        unsigned long used=0;
        run_slice(c, main_rq, hpc_rq, p, c->main_alg, core_id, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            rq_push(main_rq, p);
        }

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------
   (7) HPC THREAD
   - If HPC queue is empty and allow_hpc_steal=1, we attempt to pop
     from main queue so HPC can steal main tasks.
   ------------------------------------------------------------------ */
void* hpc_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c         = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int hpc_idx            = pack->core_id;
    free(pack);

    // negative ID for HPC timeline
    int timeline_id = -1 - hpc_idx;
    set_core_id_for_this_thread(hpc_idx);

    while(!is_time_exhausted(c)){
        block_preempt_signal();

        sigjmp_buf env;
        int ret = sigsetjmp(env, 1);
        register_jmpbuf_for_core(hpc_idx, env);

        if(ret != 0){
            fprintf(stderr, "\033[35m[HPC %d] *** PREEMPT => jumped ***\033[0m\n", hpc_idx);
        }

        // If HPC queue is empty => maybe HPC steal from main
        bool term_marker=false;
        process_t* p = rq_pop(hpc_rq, &term_marker);

        // Attempt HPC steal if HPC queue is empty
        if(!p && c->allow_hpc_steal){
            p = rq_pop(main_rq, &term_marker);
        }

        if(term_marker || !p){
            fprintf(stderr, "\033[35m[HPC %d] HPC termination => done.\033[0m\n", hpc_idx);
            break;
        }

        unsigned long used=0;
        run_slice(c, main_rq, hpc_rq, p, c->hpc_alg, timeline_id, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            rq_push(hpc_rq, p);
        }

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}
