#include "worker.h"

/* Forward helpers */
static bool is_time_exhausted(container_t* c){
    pthread_mutex_lock(&c->finish_lock);
    bool r = c->time_exhausted ||
             (c->accumulated_cpu >= c->max_cpu_time_ms);
    pthread_mutex_unlock(&c->finish_lock);
    return r;
}

/**
 * @brief Stop everything by pushing termination markers into the queues.
 */
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
                fprintf(stderr, "\033[94m[MAIN ARRIVE] P%d arrives at t=%lu => push mainRQ\033[0m\n",
                        p->id, now);
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
                fprintf(stderr, "\033[95m[HPC ARRIVE]  P%d arrives at t=%lu => push hpcRQ\033[0m\n",
                        p->id, now);
                rq_push(rq, p);
                p->arrival_time = 0;
            }
        }
    }
}

/* The central "slice run" function.
   This is basically the same logic as your original run_slice,
   but factoring out to be used by main/hpc threads.
*/
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

    while(slice_remaining > 0 && !c->time_exhausted && p->remaining_time > 0){
        unsigned long step = (slice_remaining < p->remaining_time)
                                 ? slice_remaining
                                 : p->remaining_time;

        if(step > 0){
            do_cpu_work(step, core_id, p->id);
        }

        pthread_mutex_lock(&c->finish_lock);

        p->remaining_time -= step;
        c->accumulated_cpu += step;
        c->sim_time        += step;

        slice_used += step;
        slice_remaining -= step;

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

        /* For preemptive priority, check arrivals or a higher priority arrival. */
        if(alg == ALG_PRIO_PREEMPT){
            bool got_preempted = try_preempt_if_needed(main_rq, p);
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
        /* MLFQ => demote if used the entire quantum. */
        p->mlfq_level++;
    }

    *used_ms = slice_used;
    record_timeline(c, core_id, p->id, start_ms, slice_used, preempted_flag);
    if(preempted_flag){
        fprintf(stderr, "\033[33m[CORE %d] PREEMPTED process P%d after %lu ms!\033[0m\n",
                core_id, p->id, slice_used);
    }
}

/* ---------------------------
   MAIN CORE THREAD
   --------------------------- */
void* main_core_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int core_id = pack->core_id;
    free(pack);

    while(!is_time_exhausted(c)){
        bool term_marker=false;
        process_t* p = rq_pop(main_rq, &term_marker);
        if(term_marker || !p){
            fprintf(stderr, "\033[32m[CORE %d] Termination marker => exiting.\033[0m\n", core_id);
            break;
        }
        fprintf(stderr, "\033[32m[CORE %d] Popped P%d (remaining=%lu)\033[0m\n",
                core_id, p->id, p->remaining_time);

        unsigned long used=0;
        run_slice(c, main_rq, hpc_rq, p, c->main_alg, core_id, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            fprintf(stderr, "\033[32m[CORE %d] Re-queue P%d (remaining=%lu)\033[0m\n",
                    core_id, p->id, p->remaining_time);
            rq_push(main_rq, p);
        }

        /* Check arrivals after the slice. */
        check_main_arrivals(c, main_rq);
        check_hpc_arrivals(c, hpc_rq);

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}

/* ---------------------------
   HPC THREAD
   --------------------------- */
void* hpc_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int hpc_idx = pack->core_id;
    int timeline_id = -1 - hpc_idx;  /* HPC thread gets negative ID for timeline logs. */
    free(pack);

    while(!is_time_exhausted(c)){
        /* 1) Check HPC arrivals first. */
        check_hpc_arrivals(c, hpc_rq);
        check_main_arrivals(c, main_rq);

        /* 2) HPC steal if HPC queue is empty. */
        pthread_mutex_lock(&hpc_rq->lock);
        int hpc_size = hpc_rq->size;
        pthread_mutex_unlock(&hpc_rq->lock);

        if(hpc_size == 0 && c->allow_hpc_steal){
            bool dummy=false;
            process_t* stolen = rq_pop(main_rq, &dummy);
            if(stolen && !dummy){
                fprintf(stderr, "\033[35m[HPC %d] Steal from main => P%d\033[0m\n",
                        hpc_idx, stolen->id);

                /* --- FIX for HPC-BFS ---
                   Instead of using c->main_alg for stolen process,
                   we use c->hpc_alg => e.g. BFS quantum=4
                   so we don't get stuck re-queuing 2ms slices. */
                unsigned long used=0;
                run_slice(c, main_rq, hpc_rq, stolen, /* was c->main_alg => BUG */
                          c->hpc_alg, /* <--- FIX: use HPC's scheduling for stolen procs */
                          timeline_id, &used);

                if(!is_time_exhausted(c) && stolen->remaining_time>0){
                    fprintf(stderr, "\033[35m[HPC %d] Done slice => re-push P%d to MAIN\033[0m\n",
                            hpc_idx, stolen->id);
                    rq_push(main_rq, stolen);
                }
            }
        }

        /* Re-check HPC queue size. */
        pthread_mutex_lock(&hpc_rq->lock);
        hpc_size = hpc_rq->size;
        pthread_mutex_unlock(&hpc_rq->lock);

        /* 3) If HPC queue is STILL empty => do a short "idle" to allow time to advance. */
        if(hpc_size == 0){
            if(is_time_exhausted(c)) break;

            /* Idle 1ms in simulation => do a small usleep in real-time. */
            pthread_mutex_lock(&c->finish_lock);
            c->sim_time += 1;
            c->accumulated_cpu += 1;
            if(c->accumulated_cpu >= c->max_cpu_time_ms){
                c->time_exhausted = true;
            }
            pthread_mutex_unlock(&c->finish_lock);

            usleep(3000);
            continue;
        }

        /* 4) Pop HPC queue. */
        bool term_marker=false;
        process_t* p = rq_pop(hpc_rq, &term_marker);
        if(term_marker || !p){
            fprintf(stderr, "\033[35m[HPC %d] Termination marker => exiting.\033[0m\n", hpc_idx);
            break;
        }
        fprintf(stderr, "\033[35m[HPC %d] HPC pop => P%d (remaining=%lu)\033[0m\n",
                hpc_idx, p->id, p->remaining_time);

        unsigned long used=0;
        run_slice(c, main_rq, hpc_rq, p, c->hpc_alg, timeline_id, &used);

        if(!is_time_exhausted(c) && p->remaining_time>0){
            fprintf(stderr, "\033[35m[HPC %d] Re-queue HPC P%d (remaining=%lu)\033[0m\n",
                    hpc_idx, p->id, p->remaining_time);
            rq_push(hpc_rq, p);
        }

        if(is_time_exhausted(c)){
            force_stop(c, main_rq, hpc_rq);
            break;
        }
    }
    return NULL;
}
