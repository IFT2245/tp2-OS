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
       arrival_time <= current sim_time, push them, and set their
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
            p->arrival_time = 0;  // mark as arrived
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
            p->arrival_time = 0; // mark as arrived
            rq_push(rq, p);
        }
    }
}

/* ------------------------------------------------------------------
   (4) INTERNAL UTILITY: "DISCRETE-EVENT" ADVANCE WHEN QUEUE IS EMPTY
   but ONLY if no other core is actively running a process. This
   helps avoid random differences if one core is busy but another
   jumps time forward.

   If absolutely no future arrivals => push termination marker.
   Otherwise, jump to earliest arrival time.
   ------------------------------------------------------------------ */
static void maybe_advance_time_if_idle(container_t* c, ready_queue_t* main_rq, ready_queue_t* hpc_rq){
    pthread_mutex_lock(&c->finish_lock);

    /* If main queue not empty, do nothing. */
    if(main_rq->size > 0){
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }
    /* If HPC not allowed to steal and HPC queue not empty, do nothing. */
    if(!c->allow_hpc_steal && hpc_rq->size > 0){
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }
    /* If more than 1 core is active, do nothing:
       ensures we don't jump time while another core is busy. */
    if(c->active_cores > 1){
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }

    /* Find earliest future arrival among main/hpc */
    unsigned long earliest = (unsigned long)(-1); /* big sentinel */
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

    if(earliest == (unsigned long)(-1)) {
        /* No future arrivals => push termination marker. */
        rq_push(main_rq, NULL);
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }

    /* Jump sim_time to earliest arrival. */
    c->sim_time = earliest;
    pthread_mutex_unlock(&c->finish_lock);

    /* Now that time has advanced, push arrivals if they match. */
    check_main_arrivals(c, main_rq);
    check_hpc_arrivals(c, hpc_rq);
}

/* ------------------------------------------------------------------
   (5) ACTUAL TIMESLICE RUN
   We do up to 'quantum' ms or until the process finishes. Meanwhile
   we protect c->sim_time with c->finish_lock so we don't jump from
   another thread mid-slice.  We also keep track of c->active_cores.
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
    c->active_cores++;  /* ADDED: mark this core as busy */
    unsigned long start_ms = c->sim_time;
    if(!p->responded){
        p->responded = true;
        p->first_response = start_ms;
    }
    pthread_mutex_unlock(&c->finish_lock);

    bool preempted_flag = false;
    unsigned long slice_used = 0;
    unsigned long slice_remaining = quantum;

    while(slice_remaining > 0 && !c->time_exhausted && p->remaining_time > 0){
        unsigned long step = (p->remaining_time < slice_remaining)
                           ? p->remaining_time
                           : slice_remaining;

        do_cpu_work(step, core_id, p->id);  // Real 1ms steps

        pthread_mutex_lock(&c->finish_lock);
        p->remaining_time -= step;
        c->accumulated_cpu += step;
        c->sim_time        += step;

        slice_used        += step;
        slice_remaining   -= step;

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

        /* Preemptive priority check: if new higher-prio arrived */
        if(alg == ALG_PRIO_PREEMPT){
            bool got_preempted = try_preempt_if_needed(main_rq, p);
            if(got_preempted){
                preempted_flag = true;
                break;
            }
        }

        /* Weighted Fair Queueing => update wfq_virtual_time each step. */
        if(alg == ALG_WFQ && step > 0){
            pthread_mutex_lock(&main_rq->lock);
            main_rq->wfq_virtual_time += ((double)step / p->weight);
            pthread_mutex_unlock(&main_rq->lock);
        }

        if(c->time_exhausted) break;
    }

    /* If MLFQ: if we used the entire quantum and not finished => next queue level. */
    if(alg == ALG_MLFQ && p->remaining_time>0 && slice_used == quantum){
        p->mlfq_level++;
    }

    *used_ms = slice_used;
    record_timeline(c, core_id, p->id, start_ms, slice_used, preempted_flag);

    /* If preempted, log a message. */
    if(preempted_flag){
        fprintf(stderr, "\033[33m[CORE %d] PREEMPTED process P%d after %lu ms!\033[0m\n",
                core_id, p->id, slice_used);
    }

    /* ADDED: done with slice => mark this core as no longer active. */
    pthread_mutex_lock(&c->finish_lock);
    c->active_cores--;
    pthread_mutex_unlock(&c->finish_lock);
}

/* ------------------------------------------------------------------
   (6) MAIN CORE THREAD
   - If queue empty => maybe_advance_time_if_idle
   - Pop => run_slice => re-insert if needed => check arrivals
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
            fprintf(stderr, "\033[31m[CORE %d] *** IMMEDIATE PREEMPTION => jumped back ***\033[0m\n",
                    core_id);
        }

        /* If queue is empty, try discrete-event jump. */
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

        /* Check arrivals after sim_time advanced. */
        check_main_arrivals(c, main_rq);
        check_hpc_arrivals(c, hpc_rq);

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------
   (7) HPC THREAD
   Similar logic, except HPC queue can be empty while HPC might
   or might not steal main tasks (allow_hpc_steal).
   ------------------------------------------------------------------ */
void* hpc_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c         = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int hpc_idx            = pack->core_id;
    int timeline_id        = -1 - hpc_idx;
    free(pack);

    set_core_id_for_this_thread(hpc_idx);

    while(!is_time_exhausted(c)){
        block_preempt_signal();

        sigjmp_buf env;
        int ret = sigsetjmp(env, 1);
        register_jmpbuf_for_core(hpc_idx, env);

        if(ret != 0){
            fprintf(stderr, "\033[35m[HPC %d] *** PREEMPT => jumped ***\033[0m\n", hpc_idx);
        }

        /* If HPC queue is empty => discrete-event jump only if we can't steal. */
        if(hpc_rq->size == 0 && !c->allow_hpc_steal){
            maybe_advance_time_if_idle(c, main_rq, hpc_rq);
        }

        bool term_marker=false;
        process_t* p = rq_pop(hpc_rq, &term_marker);
        if(term_marker || !p){
            fprintf(stderr, "\033[35m[HPC %d] HPC termination => done.\033[0m\n", hpc_idx);
            break;
        }

        unsigned long used=0;
        run_slice(c, main_rq, hpc_rq, p, c->hpc_alg, timeline_id, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            rq_push(hpc_rq, p);
        }

        /* HPC thread can also check arrivals. */
        if(!c->allow_hpc_steal){
            check_main_arrivals(c, main_rq);
        }
        check_hpc_arrivals(c, hpc_rq);

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}