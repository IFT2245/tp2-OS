#include "worker.h"

/* Forward helpers */
static bool is_time_exhausted(container_t* c){
    pthread_mutex_lock(&c->finish_lock);
    bool r = c->time_exhausted || (c->accumulated_cpu >= c->max_cpu_time_ms);
    pthread_mutex_unlock(&c->finish_lock);
    return r;
}

/**
 * @brief Stop everything by pushing termination markers.
 */
static void force_stop(container_t* c, ready_queue_t* rq_main, ready_queue_t* rq_hpc){
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

/**
 * @brief Perform one CPU slice for a given process `p` on core `core_id`.
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

        /* Actual CPU simulation step: */
        do_cpu_work(step, core_id, p->id);

        pthread_mutex_lock(&c->finish_lock);

        p->remaining_time -= step;
        c->accumulated_cpu += step;
        c->sim_time        += step;

        slice_used       += step;
        slice_remaining  -= step;

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

        /* Preemptive check for PRIO_PREEMPT: */
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

    /* MLFQ => demote if used the entire quantum but not finished. */
    if(alg == ALG_MLFQ && p->remaining_time > 0 && slice_used == quantum){
        p->mlfq_level++;
    }

    *used_ms = slice_used;
    record_timeline(c, core_id, p->id, start_ms, slice_used, preempted_flag);

    if(preempted_flag){
        fprintf(stderr, "\033[33m[CORE %d] PREEMPTED process P%d after %lu ms!\033[0m\n",
                core_id, p->id, slice_used);
    }
}

void* main_core_thread(void* arg){
    core_thread_pack_t* pack = (core_thread_pack_t*)arg;
    container_t* c         = pack->container;
    ready_queue_t* main_rq = pack->qs.main_rq;
    ready_queue_t* hpc_rq  = pack->qs.hpc_rq;
    int core_id            = pack->core_id;
    free(pack);

    while(!is_time_exhausted(c)){
        bool term_marker = false;
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

        /* Check arrivals after the slice: */
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
    int hpc_idx            = pack->core_id;
    int timeline_id        = -1 - hpc_idx;
    free(pack);

    while(!is_time_exhausted(c)){
        /* 1) Check arrivals: */
        check_hpc_arrivals(c, hpc_rq);
        check_main_arrivals(c, main_rq);

        /* 2) HPC steal if HPC queue empty and allow_hpc_steal. */
        pthread_mutex_lock(&hpc_rq->lock);
        int hpc_size = hpc_rq->size;
        pthread_mutex_unlock(&hpc_rq->lock);

        if(hpc_size == 0 && c->allow_hpc_steal){
            bool dummy = false;
            process_t* stolen = rq_pop(main_rq, &dummy);
            if(stolen && !dummy){
                fprintf(stderr, "\033[35m[HPC %d] Steal from main => P%d\033[0m\n",
                        hpc_idx, stolen->id);

                unsigned long used=0;
                /* HPC BFS uses HPC scheduling, not main's. */
                run_slice(c, main_rq, hpc_rq, stolen, c->hpc_alg, timeline_id, &used);

                if(!is_time_exhausted(c) && stolen->remaining_time>0){
                    fprintf(stderr, "\033[35m[HPC %d] Done slice => re-push P%d to MAIN\033[0m\n",
                            hpc_idx, stolen->id);
                    rq_push(main_rq, stolen);
                }
            }
        }

        /* Re-check HPC queue: */
        pthread_mutex_lock(&hpc_rq->lock);
        hpc_size = hpc_rq->size;
        pthread_mutex_unlock(&hpc_rq->lock);

        /* 3) If HPC queue still empty => idle 1ms (faster idle to avoid timeouts). */
        if(hpc_size == 0){
            if(is_time_exhausted(c)) break;
            pthread_mutex_lock(&c->finish_lock);
            c->sim_time += 1;
            c->accumulated_cpu += 1;
            if(c->accumulated_cpu >= c->max_cpu_time_ms){
                c->time_exhausted = true;
            }
            pthread_mutex_unlock(&c->finish_lock);

            /* Sleep a small amount of real time to keep HPC BFS from timing out. */
            usleep(1000);  // 1ms real-time idle
            continue;
        }

        /* 4) Pop HPC queue: */
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


/* Tracks slow-mode on/off globally. */
// This variable controls slow vs. fast sleep logic:
static int g_slowMode = 0;
static int g_bonusTest = 0;
void set_slow_mode(const int onOff) {
    // Just record the local preference, no shell calls:
    g_slowMode = (onOff != 0);
}
void set_bonus_test(const int onOff) {
    // *** ADDED FOR BONUS TEST ***
    g_bonusTest = (onOff != 0);
}
int is_slow_mode(void) {
    return g_slowMode;
}
int is_bonus_test(void) {
    // *** ADDED FOR BONUS TEST ***
    return g_bonusTest;
}

/**
 * @brief Simulates CPU work by sleeping. In fast mode, small microsecond sleeps.
 *        In slow mode, large sleeps. In bonus-test mode, we demonstrate concurrency
 *        via an external shell call or a longer time-based approach.
 */
void do_cpu_work(unsigned long ms, int core_id, int proc_id)
{
    if (!is_slow_mode() && !is_bonus_test()) {
        // FAST mode => microsecond-level sleeps
        log_info("do_cpu_work(FAST): %lu ms => usleep(%lu * 1000)", ms, ms);
        usleep((useconds_t)(ms * 1000U));
        return;
    }

    if (is_slow_mode() && !is_bonus_test()) {
        // SLOW mode => 2 seconds per simulated ms (increase from 1 to 2 to avoid timeouts)
        for (unsigned long i = 0; i < ms; i++) {
            log_info("do_cpu_work(SLOW): sleeping 2s for ms=%lu (core=%d, proc=%d)",
                     i, core_id, proc_id);
            sleep(2);
        }
        return;
    }

    if (is_bonus_test()) {
        /*
         * BONUS TEST mode:
         * Example approach: call an external shell that runs “timeout 10 sleep <ms>”
         * or run repeated “sleep 1” calls to mimic concurrency checks in
         * shell-tp1-implementation. The snippet below does a single big sleep with
         * an overall timeout. Adjust as desired to match your concurrency tests.
         */
        char cmd[256];
        // Example: for a 10-second limit, request “sleep <ms>”.
        // If ms is large, it will be cut short by ‘timeout 10’.
        snprintf(cmd, sizeof(cmd),
                 "echo \"timeout 10 sleep %lu\" | ../../libExtern/shell-tp1-implementation",
                 ms);

        log_info("[BONUS] do_cpu_work => %s", cmd);
        int ret = system(cmd);
        if (ret != 0) {
            log_warn("[BONUS] shell command returned non-zero (%d), might have timed out or failed", ret);
        }
        return;
    }
}
