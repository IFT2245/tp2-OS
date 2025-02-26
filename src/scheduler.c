#include "scheduler.h"
#include "ready_queue.h"
#include "worker.h"
#include "os.h"
#include "stats.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* We'll assume max of 4 cores for BFS/MLFQ demonstration. */
#ifndef MAX_CORES
#define MAX_CORES 4
#endif

/* ---------- Global scheduling states ---------- */
static scheduler_alg_t  g_current_alg = ALG_CFS;
static int              g_num_cores   = 1;
static int              g_running     = 0;

/*
   The global simulated time in "ms" (not real ms, but a time-step
   that increments with each partial slice).
*/
static uint64_t         g_sim_time    = 0;
static pthread_mutex_t  g_sim_time_lock = PTHREAD_MUTEX_INITIALIZER;

/* track final stats in a single structure. */
static schedule_stats_t g_stats;

/* We keep a pointer to the list of processes used in the run. */
static process_t* g_process_list = NULL;
static int        g_list_count   = 0;

/*
   BFS quantum is stored so BFS tests can see what BFS used.
   This is set in get_dynamic_quantum() if BFS is selected.
*/
static unsigned long g_bfs_quantum = 4; /* default fallback */

/* Provide BFS quantum to BFS tests. */
unsigned long scheduler_get_bfs_quantum(void) {
    return g_bfs_quantum;
}

/* Access global sim time (used by ready_queue for e.g. HRRN, MLFQ) */
uint64_t get_global_sim_time(void) {
    pthread_mutex_lock(&g_sim_time_lock);
    uint64_t t = g_sim_time;
    pthread_mutex_unlock(&g_sim_time_lock);
    return t;
}

/* Resets scheduling stats & sim time at run start. */
static void reset_schedule_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
    g_sim_time = 0;
}

/* Compute final stats after all processes finish. */
static void finalize_stats(void) {
    for(int i=0; i<g_list_count; i++) {
        process_t* P = &g_process_list[i];
        uint64_t at  = P->arrival_time;
        uint64_t st  = P->first_response;
        uint64_t et  = P->end_time;

        uint64_t wait = (st > at) ? (st - at) : 0ULL;
        uint64_t tat  = (et > at) ? (et - at) : 0ULL;
        uint64_t resp = wait;

        g_stats.total_wait += wait;
        g_stats.total_tat  += tat;
        g_stats.total_resp += resp;
    }
    g_stats.total_count = g_list_count;

    if(g_stats.total_count > 0) {
        g_stats.avg_wait       = (double)g_stats.total_wait / (double)g_stats.total_count;
        g_stats.avg_turnaround = (double)g_stats.total_tat  / (double)g_stats.total_count;
        g_stats.avg_response   = (double)g_stats.total_resp / (double)g_stats.total_count;
    }
    g_stats.preemptions  = g_stats.total_preempts;
    g_stats.total_procs   = g_stats.total_count;
}

/*
   Returns a dynamic quantum for each preemptive scheduling mode.
   BFS => store in g_bfs_quantum, so BFS test can see the actual timeslice.
*/
static unsigned long get_dynamic_quantum(scheduler_alg_t alg) {
    size_t rq_size = ready_queue_size();
    switch(alg) {
        case ALG_RR: {
            /* Example dynamic approach for RR. */
            const unsigned long baseQuantum = 2;
            unsigned long dynamic_part = (unsigned long)(rq_size / 2);
            return baseQuantum + dynamic_part;
        }
        case ALG_BFS:
            /*
               BFS => choose base slice=4, maybe +1 if queue is big.
               This is just an example. You can do what you want here.
            */
            g_bfs_quantum = 4 + (rq_size > 5 ? 1 : 0);
            return g_bfs_quantum;

        case ALG_CFS_SRTF:
        case ALG_STRF:
        case ALG_HRRN_RT:
        case ALG_MLFQ:
            /* short fixed slice => 2 ms. */
            return 2;

        default:
            /* Non-preemptive => run to completion => quantum=0 => no preemption. */
            return 0;
    }
}

/*
   The scheduling loop for each core:
   - pop from ready_queue
   - run partial timeslice
   - if remain>0 => requeue => preempt
*/
static void* core_thread_func(void* arg) {
    long core_id = (long)arg;

    while(g_running) {
        if(os_concurrency_stop_requested()) {
            break;
        }

        process_t* p = ready_queue_pop();
        if(!g_running || !p) {
            /* if queue empty or sentinel => skip */
            continue;
        }

        uint64_t real_t = os_time();
        if(stats_get_speed_mode() == 0) {
            printf("\033[93m[time=%llu ms] => container=1 core=%ld => scheduling process=%p\n"
                   "   => burst_time=%lu, prio=%d, vruntime=%llu, remain=%llu, timesScheduled=%d\n\033[0m",
                   (unsigned long long)real_t,
                   core_id, (void*)p,
                   (unsigned long)p->burst_time,
                   p->priority,
                   (unsigned long long)p->vruntime,
                   (unsigned long long)p->remaining_time,
                   p->times_owning_core);
            usleep(300000);
        }

        if(!p->responded) {
            p->responded       = 1;
            p->first_response  = get_global_sim_time();
        }
        p->times_owning_core++;

        /* Decide if preemptive => choose quantum. */
        unsigned long q = get_dynamic_quantum(g_current_alg);
        int preemptive = (q > 0) ? 1 : 0;

        /* slice = min(q, remaining_time) if preemptive, else entire remain. */
        unsigned long slice = preemptive
                              ? ( (p->remaining_time > q) ? q : p->remaining_time )
                              : p->remaining_time;

        /* run partial. */
        simulate_process_partial(p, slice, (int)core_id);

        /* update global simulated time. */
        pthread_mutex_lock(&g_sim_time_lock);
        g_sim_time += slice;
        uint64_t now_sim = g_sim_time;
        pthread_mutex_unlock(&g_sim_time_lock);

        p->remaining_time -= slice;

        /* If using CFS or CFS-SRTF => update vruntime. */
        if(g_current_alg == ALG_CFS || g_current_alg == ALG_CFS_SRTF) {
            p->vruntime += slice;
        }

        /* still not finished => requeue => preempt++ */
        if(preemptive && p->remaining_time > 0) {
            __sync_fetch_and_add(&g_stats.total_preempts, 1ULL);

            if(stats_get_speed_mode()==0) {
                printf("\033[94m   => PREEMPT => process=%p => new remain=%llu => preemptions=%llu\n\033[0m",
                       (void*)p,
                       (unsigned long long)p->remaining_time,
                       (unsigned long long)g_stats.total_preempts);
                usleep(300000);
            }

            /* degrade to next queue if MLFQ */
            if(g_current_alg == ALG_MLFQ) {
                p->mlfq_level++;
            }
            ready_queue_push(p);
        }
        else {
            /* The process is done. */
            p->end_time = now_sim;
            if(stats_get_speed_mode()==0) {
                printf("\033[92m   => FINISH => process=%p => total CPU used=%lu ms => time=%llu ms\n\033[0m",
                       (void*)p,
                       (unsigned long)slice,
                       (unsigned long long)os_time());
                usleep(300000);
            }
        }
    }
    return NULL;
}

/* ---------- Public Functions ---------- */

void scheduler_select_algorithm(scheduler_alg_t a) {
    g_current_alg = a;
}

void scheduler_run(process_t* list, int count) {
    reset_schedule_stats();
    if(!list || count<=0) return;

    /* HPC overshadow/overlay => special handling. */
    if(g_current_alg == ALG_HPC_OVERSHADOW) {
        g_stats.HPC_over_mode = 1;
        if(stats_get_speed_mode()==0) {
            printf("\n\033[95m╔══════════════════════════════════════════════╗\n");
            printf(         "║   SCHEDULE => HPC-OVERSHADOW (special mode)   ║\n");
            printf(         "╚══════════════════════════════════════════════╝\033[0m\n");
            usleep(300000);
        }
        os_run_hpc_overshadow();
        return;
    }
    else if(g_current_alg == ALG_HPC_OVERLAY) {
        g_stats.HPC_overlay_mode = 1;
        if(stats_get_speed_mode()==0) {
            printf("\n\033[95m╔══════════════════════════════════════════════╗\n");
            printf(         "║    SCHEDULE => HPC-OVERLAY (special mode)     ║\n");
            printf(         "╚══════════════════════════════════════════════╝\033[0m\n");
            usleep(300000);
        }
        os_run_hpc_overlay();
        return;
    }

    /* BFS or MLFQ => 2 cores, else 1. */
    if(g_current_alg == ALG_BFS || g_current_alg == ALG_MLFQ) {
        g_num_cores = 2;
    } else {
        g_num_cores = 1;
    }

    if(stats_get_speed_mode()==0) {
        printf("\n\033[95m╔══════════════════════════════════════════════╗\n");
        printf(       "║   SCHEDULE => alg=%d => #processes=%d          \n", g_current_alg, count);
        printf(       "║   RealTime start => %llu ms                   \n", (unsigned long long)os_time());
        printf(       "╚══════════════════════════════════════════════╝\033[0m\n");
        usleep(300000);
    }

    ready_queue_init_policy(g_current_alg);

    g_process_list = list;
    g_list_count   = count;
    g_running      = 1;

    /* push all processes into the queue. */
    for(int i=0; i<count; i++) {
        ready_queue_push(&list[i]);
    }

    pthread_t tid[MAX_CORES];
    int n = (g_num_cores > MAX_CORES) ? MAX_CORES : g_num_cores;

    /* spawn each core thread. */
    for(int i=0; i<n; i++) {
        pthread_create(&tid[i], NULL, core_thread_func, (void*)(long)i);
    }

    /* wait while queue is not empty */
    while(ready_queue_size() > 0) {
        usleep(200000);
        if(os_concurrency_stop_requested()) {
            break;
        }
    }

    /* end scheduling => push sentinel => join threads */
    g_running = 0;
    for(int i=0; i<n; i++) {
        ready_queue_push(NULL);
    }
    for(int i=0; i<n; i++) {
        pthread_join(tid[i], NULL);
    }

    ready_queue_destroy();
    finalize_stats();

    if(stats_get_speed_mode()==0) {
        uint64_t total_time = get_global_sim_time();
        printf("\033[96m╔══════════════════════════════════════════════╗\n");
        printf(       "║ SCHEDULE END => alg=%d => totalTime=%llums    \n",
                      g_current_alg, (unsigned long long)total_time);
        printf(       "║ Stats: preemptions=%llu, totalProcs=%llu     \n",
                      (unsigned long long)g_stats.total_preempts, (unsigned long long)g_stats.total_procs);
        printf(       "║ AvgWait=%.2f, AvgTAT=%.2f, AvgResp=%.2f       \n",
                      g_stats.avg_wait, g_stats.avg_turnaround, g_stats.avg_response);
        printf(       "╚══════════════════════════════════════════════╝\033[0m\n");
        usleep(300000);
    }

    g_process_list = NULL;
    g_list_count   = 0;
}

void scheduler_fetch_report(sched_report_t* out) {
    if(!out) return;
    /* HPC overshadow/overlay => zero normal stats. */
    if(g_stats.HPC_over_mode || g_stats.HPC_overlay_mode) {
        out->avg_wait       = 0.0;
        out->avg_turnaround = 0.0;
        out->avg_response   = 0.0;
        out->preemptions    = 0ULL;
        out->total_procs    = 0ULL;
    } else {
        out->avg_wait       = g_stats.avg_wait;
        out->avg_turnaround = g_stats.avg_turnaround;
        out->avg_response   = g_stats.avg_response;
        out->preemptions    = g_stats.preemptions;
        out->total_procs    = g_stats.total_procs;
    }
}
