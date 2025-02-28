#include "worker.h"
#include "ready_queue.h"
#include "scheduler.h"
#include "../lib/library.h"
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward helpers. */
static bool is_time_exhausted(container_t* c){
    pthread_mutex_lock(&c->finish_lock);
    bool r = c->time_exhausted || (c->accumulated_cpu >= c->max_cpu_time_ms);
    pthread_mutex_unlock(&c->finish_lock);
    return r;
}

static void force_stop(const container_t* c, ready_queue_t* rq_main, ready_queue_t* rq_hpc){
    for(int i=0; i<c->nb_cores; i++){
        rq_push(rq_main, NULL);
    }
    for(int i=0; i<c->nb_hpc_threads; i++){
        rq_push(rq_hpc, NULL);
    }
}

static void check_main_arrivals(container_t* c, ready_queue_t* rq){
    pthread_mutex_lock(&c->finish_lock);
    unsigned long now = c->sim_time;
    pthread_mutex_unlock(&c->finish_lock);

    for(int i=0; i<c->main_count; i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time>0 && p->arrival_time <= now){
            fprintf(stderr, "\033[94m[MAIN ARRIVE] P%d arrives at t=%lu => push mainRQ\033[0m\n",
                    p->id, now);
            rq_push(rq, p);
            p->arrival_time = 0;
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
            rq_push(rq, p);
            p->arrival_time = 0;
        }
    }
}

static void run_slice(container_t* c, ready_queue_t* main_rq, ready_queue_t* hpc_rq,
                      process_t* p, scheduler_alg_t alg,
                      int core_id, unsigned long* used_ms)
{
    *used_ms = 0;
    if(!p || p->remaining_time == 0) return;

    unsigned long quantum = get_quantum(alg, p);

    pthread_mutex_lock(&c->finish_lock);
    unsigned long start_ms = c->sim_time;
    if(!p->responded){
        p->responded = true;
        p->first_response = start_ms;
    }
    pthread_mutex_unlock(&c->finish_lock);

    bool preempted_flag = false;
    unsigned long slice_used = 0;
    unsigned long slice_remaining = quantum;

    while(slice_remaining>0 && !c->time_exhausted && p->remaining_time>0){
        unsigned long step = (p->remaining_time < slice_remaining)
                           ? p->remaining_time
                           : slice_remaining;
        /*
         * This call will *unblock* SIGALRM, so if the timer fires mid-burst,
         * we do siglongjmp => skip the rest of run_slice.
         */
        do_cpu_work(step, core_id, p->id);

        /* If we got here, that means no preemption occurred. We can safely update. */
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

        if(alg == ALG_PRIO_PREEMPT){
            bool got_preempted = try_preempt_if_needed(main_rq, p);
            if(got_preempted){
                preempted_flag = true;
                break;
            }
        }
        if(c->time_exhausted) break;
    }

    if(alg == ALG_MLFQ && p->remaining_time>0 && slice_used==quantum){
        p->mlfq_level++;
    }

    *used_ms = slice_used;
    record_timeline(c, core_id, p->id, start_ms, slice_used, preempted_flag);
    if(preempted_flag){
        fprintf(stderr, "\033[33m[CORE %d] PREEMPTED process P%d after %lu ms!\033[0m\n",
                core_id, p->id, slice_used);
    }
}

/*
 * The main core thread.
 * We do a sigsetjmp at each iteration, register the buffer,
 * then do the scheduling. If preempted mid do_cpu_work(),
 * siglongjmp is invoked, returning here with ret!=0.
 */
void* main_core_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c         = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int core_id            = pack->core_id;
    free(pack);

    set_core_id_for_this_thread(core_id);

    while(!is_time_exhausted(c)){
        /* 1) Block SIGALRM while we manipulate the scheduling queue. */
        block_preempt_signal();

        sigjmp_buf env;
        int ret = sigsetjmp(env, 1);
        register_jmpbuf_for_core(core_id, env);

        if(ret != 0){
            fprintf(stderr, "\033[31m[CORE %d] *** IMMEDIATE PREEMPTION => jumped back ***\033[0m\n",
                    core_id);
            /* We skip the remainder of run_slice, so the partial usage is
               not yet accounted for in c->accumulated_cpu.
               If you want to track partial usage, you'd do smaller loops in run_slice,
               or do a second clock read after siglongjmp.
               This can get advanced, so let's keep it simpler here. */
        }

        bool term_marker = false;
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

        check_main_arrivals(c, main_rq);
        check_hpc_arrivals(c, hpc_rq);

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}

void* hpc_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c         = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int hpc_idx            = pack->core_id;  // or some HPC ID offset
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

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}
