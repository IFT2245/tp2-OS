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
   (A) UTILITY: Check if container has exceeded time
   ------------------------------------------------------------------ */
static bool is_time_exhausted(container_t* c){
    pthread_mutex_lock(&c->finish_lock);
    bool r = (c->time_exhausted || (c->accumulated_cpu >= c->max_cpu_time_ms));
    pthread_mutex_unlock(&c->finish_lock);
    return r;
}

/* ------------------------------------------------------------------
   (B) UTILITY: Push termination markers into queues
   ------------------------------------------------------------------ */
static void force_stop(const container_t* c,
                       ready_queue_t* rq_main,
                       ready_queue_t* rq_hpc){
    for(int i=0; i<c->nb_cores; i++){
        rq_push(rq_main, NULL);
    }
    for(int i=0; i<c->nb_hpc_threads; i++){
        rq_push(rq_hpc, NULL);
    }
}

/* ------------------------------------------------------------------
   (C) UTILITY: Check arrivals whose arrival_time <= sim_time
   ------------------------------------------------------------------ */
static void check_main_arrivals(container_t* c, ready_queue_t* rq){
    pthread_mutex_lock(&c->finish_lock);
    unsigned long now = c->sim_time;
    pthread_mutex_unlock(&c->finish_lock);

    for(int i=0; i<c->main_count; i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time <= now){
            fprintf(stderr,"\033[94m[MAIN ARRIVE] P%d arrives at t=%lu => push mainRQ\033[0m\n",
                    p->id, now);
            p->arrival_time = 0;
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
            fprintf(stderr,"\033[95m[HPC ARRIVE]  P%d arrives at t=%lu => push hpcRQ\033[0m\n",
                    p->id, now);
            p->arrival_time = 0;
            rq_push(rq, p);
        }
    }
}

/* ------------------------------------------------------------------
   (D) UTILITY: "Discrete-event" jump if both RQs empty + no active cores
   ------------------------------------------------------------------ */
static void maybe_advance_time_if_idle(container_t* c,
                                       ready_queue_t* main_rq,
                                       ready_queue_t* hpc_rq)
{
    pthread_mutex_lock(&c->finish_lock);

    // if either queue non-empty => no jump
    if(main_rq->size>0) {
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }
    if(!c->allow_hpc_steal && hpc_rq->size>0) {
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }
    // if any core is running => no jump
    if(c->active_cores > 0) {
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }

    // find earliest future arrival (main or HPC)
    unsigned long earliest = (unsigned long)-1;

    for(int i=0; i<c->main_count; i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time<earliest){
            earliest = p->arrival_time;
        }
    }
    for(int i=0; i<c->hpc_count; i++){
        process_t* p = &c->hpc_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time<earliest){
            earliest = p->arrival_time;
        }
    }

    if(earliest == (unsigned long)-1){
        // no future arrivals => forcibly push termination
        for(int i=0;i<c->nb_cores;i++) rq_push(main_rq,NULL);
        for(int i=0;i<c->nb_hpc_threads;i++) rq_push(hpc_rq,NULL);
        pthread_mutex_unlock(&c->finish_lock);
        return;
    }

    // jump sim_time
    c->sim_time = earliest;
    pthread_mutex_unlock(&c->finish_lock);

    // now check arrivals
    check_main_arrivals(c, main_rq);
    check_hpc_arrivals(c, hpc_rq);
}

/* ------------------------------------------------------------------
   (E) Run a timeslice in 1ms increments
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
    if(!p || p->remaining_time==0) return;

    unsigned long q = get_quantum(alg, p);

    pthread_mutex_lock(&c->finish_lock);
    c->active_cores++;
    unsigned long start_ms = c->sim_time;
    if(!p->responded){
        p->responded      = true;
        p->first_response = start_ms;
    }
    pthread_mutex_unlock(&c->finish_lock);

    bool preempted_flag=false;
    unsigned long slice_used=0;

    while(slice_used < q && !c->time_exhausted && p->remaining_time>0){
        do_cpu_work(1, core_id, p->id);

        pthread_mutex_lock(&c->finish_lock);
        p->remaining_time--;
        c->accumulated_cpu++;
        c->sim_time++;
        slice_used++;

        if(p->remaining_time==0){
            p->end_time = p->first_response + p->burst_time;
            c->remaining_count--;
            if(c->remaining_count<=0) c->time_exhausted=true;
        }
        if(c->accumulated_cpu>=c->max_cpu_time_ms){
            c->time_exhausted=true;
        }
        pthread_mutex_unlock(&c->finish_lock);

        // if WFQ => update each ms
        if(alg==ALG_WFQ){
            pthread_mutex_lock(&main_rq->lock);
            main_rq->wfq_virtual_time+=(1.0/p->weight);
            pthread_mutex_unlock(&main_rq->lock);
        }
        // preemptive priority => check each ms
        if(alg==ALG_PRIO_PREEMPT){
            bool got_preempted = try_preempt_if_needed(main_rq, p);
            if(got_preempted){
                preempted_flag=true;
                break;
            }
        }
        // arrivals each ms
        check_main_arrivals(c, main_rq);
        check_hpc_arrivals(c, hpc_rq);

        if(is_time_exhausted(c)) break;
    }

    // MLFQ => if used entire quantum => next level
    if(alg==ALG_MLFQ && p->remaining_time>0 && slice_used==q){
        p->mlfq_level++;
    }

    *used_ms = slice_used;
    record_timeline(c, core_id, p->id, start_ms, slice_used, preempted_flag);

    if(preempted_flag){
        fprintf(stderr,"\033[33m[CORE %d] PREEMPTED process P%d after %lu ms!\033[0m\n",
                core_id, p->id, slice_used);
    }

    pthread_mutex_lock(&c->finish_lock);
    c->active_cores--;
    pthread_mutex_unlock(&c->finish_lock);
}

/* ------------------------------------------------------------------
   (F) MAIN CORE THREAD
   ------------------------------------------------------------------ */
void* main_core_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int core_id = pack->core_id;
    free(pack);

    set_core_id_for_this_thread(core_id);

    while(!is_time_exhausted(c)){
        block_preempt_signal();
        sigjmp_buf env;
        int ret=sigsetjmp(env,1);
        register_jmpbuf_for_core(core_id,env);
        if(ret!=0){
            fprintf(stderr,"\033[31m[CORE %d] *** IMMEDIATE PREEMPT => jumped back ***\033[0m\n",
                    core_id);
        }

        maybe_advance_time_if_idle(c, main_rq, hpc_rq);

        bool term_marker=false;
        process_t* p = rq_pop(main_rq,&term_marker);
        if(term_marker || !p){
            fprintf(stderr,"\033[32m[CORE %d] Termination => done.\033[0m\n",core_id);
            break;
        }

        unsigned long used=0;
        run_slice(c, main_rq, hpc_rq, p, c->main_alg, core_id, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            rq_push(main_rq,p);
        }
        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------
   (G) HPC THREAD => HPC BFS improvements:
       we call maybe_advance_time_if_idle before & after each slice
   ------------------------------------------------------------------ */
void* hpc_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c=pack->container;
    ready_queue_t* main_rq=pack->qs.main_rq;
    ready_queue_t* hpc_rq =pack->qs.hpc_rq;
    int hpc_idx=pack->core_id;
    free(pack);

    int timeline_id=(-1 - hpc_idx);
    set_core_id_for_this_thread(hpc_idx);

    while(!is_time_exhausted(c)){
        block_preempt_signal();
        sigjmp_buf env;
        int ret=sigsetjmp(env,1);
        register_jmpbuf_for_core(hpc_idx,env);
        if(ret!=0){
            fprintf(stderr,"\033[35m[HPC %d] *** PREEMPT => jumped ***\033[0m\n",hpc_idx);
        }

        /* Before picking next process, let’s see if we can jump time. */
        maybe_advance_time_if_idle(c, main_rq, hpc_rq);

        bool term_marker=false;
        process_t* p = rq_pop(hpc_rq, &term_marker);

        /* HPC steal if HPC queue empty + allow_hpc_steal=1 */
        if(!p && c->allow_hpc_steal){
            p = rq_pop(main_rq,&term_marker);
        }

        if(term_marker || !p){
            fprintf(stderr,"\033[35m[HPC %d] HPC termination => done.\033[0m\n",hpc_idx);
            break;
        }

        unsigned long used=0;
        run_slice(c, main_rq, hpc_rq, p, c->hpc_alg, timeline_id, &used);

        /* After the slice, do another maybe_advance_time_if_idle
           so HPC-only BFS can jump forward if no one’s ready. */
        maybe_advance_time_if_idle(c, main_rq, hpc_rq);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            rq_push(hpc_rq,p);
        }

        if(is_time_exhausted(c)){
            force_stop(c,main_rq,hpc_rq);
            break;
        }
    }
    return NULL;
}
