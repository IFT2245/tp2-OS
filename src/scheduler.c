#include "scheduler.h"
#include "ready_queue.h"
#include "worker.h"
#include "os.h"
#include "stats.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef MAX_CORES
#define MAX_CORES 4
#endif

static scheduler_alg_t g_current_alg = ALG_CFS;
static int             g_num_cores   = 1;
static int             g_running     = 0;

/* global sim_time => increment by slices to compute wait, TAT, etc. */
static uint64_t        g_sim_time = 0;
static pthread_mutex_t g_sim_time_lock = PTHREAD_MUTEX_INITIALIZER;

uint64_t get_global_sim_time(void) {
    pthread_mutex_lock(&g_sim_time_lock);
    uint64_t t = g_sim_time;
    pthread_mutex_unlock(&g_sim_time_lock);
    return t;
}

/* accumulators for stats. */
static double gAvgWait = 0.0, gAvgTAT = 0.0, gAvgResp = 0.0;
static unsigned long long gPreemptions = 0ULL, gProcs = 0ULL;

static uint64_t g_total_wait = 0ULL;
static uint64_t g_total_tat  = 0ULL;
static uint64_t g_total_resp = 0ULL;
static unsigned long long g_total_preempts = 0ULL;
static int                g_total_count    = 0;

static int HPC_over_mode = 0;
static process_t* g_process_list = NULL;
static int        g_list_count   = 0;

static void sim_sleep_scheduler(unsigned int us){
    int sm = stats_get_speed_mode();
    if(sm==1){
        usleep(us/10 + 1);
    } else {
        usleep(us);
    }
}

static void reset_accumulators(void) {
    gAvgWait       = 0.0;
    gAvgTAT        = 0.0;
    gAvgResp       = 0.0;
    gPreemptions   = 0ULL;
    gProcs         = 0ULL;

    g_total_wait   = 0ULL;
    g_total_tat    = 0ULL;
    g_total_resp   = 0ULL;
    g_total_preempts = 0ULL;
    g_total_count  = 0;

    HPC_over_mode  = 0;
    g_sim_time     = 0;
}

void scheduler_select_algorithm(scheduler_alg_t a) {
    g_current_alg = a;
}

static void* core_thread_func(void* arg) {
    long core_id = (long)arg;
    unsigned long quantum = 2; /* small quantum for preemptive algs */

    while(g_running) {
        process_t* p = ready_queue_pop();
        if(!g_running || !p) {
            continue;
        }
        uint64_t real_t = os_time();
        printf("\033[93m[time=%llu ms] => container=1 core=%ld => scheduling processPtr=%p\n"
               "   => burst_time=%lu, prio=%d, vruntime=%llu, remain=%llu, timesScheduled=%d\033[0m\n",
               (unsigned long long)real_t,
               core_id, (void*)p,
               (unsigned long)p->burst_time,
               p->priority,
               (unsigned long long)p->vruntime,
               (unsigned long long)p->remaining_time,
               p->times_owning_core);
        sim_sleep_scheduler(300000);

        if(!p->responded) {
            p->responded = 1;
            p->first_response = get_global_sim_time();
        }
        p->times_owning_core++;

        int preemptive = 0;
        switch(g_current_alg) {
        case ALG_RR:
        case ALG_BFS:
        case ALG_CFS_SRTF:
        case ALG_STRF:
        case ALG_HRRN_RT:
        case ALG_MLFQ:
            preemptive = 1;
            break;
        default:
            preemptive = 0;
            break;
        }

        unsigned long slice = 0;
        if(preemptive) {
            if(p->remaining_time > quantum) {
                slice = quantum;
            } else {
                slice = p->remaining_time;
            }
        } else {
            slice = p->remaining_time;
        }

        simulate_process_partial(p, slice, (int)core_id);

        /* increment g_sim_time. */
        pthread_mutex_lock(&g_sim_time_lock);
        g_sim_time += slice;
        uint64_t now_sim = g_sim_time;
        pthread_mutex_unlock(&g_sim_time_lock);

        p->remaining_time -= slice;
        if(g_current_alg == ALG_CFS || g_current_alg == ALG_CFS_SRTF) {
            p->vruntime += slice;
        }

        if(preemptive && p->remaining_time > 0) {
            __sync_fetch_and_add(&g_total_preempts, 1ULL);
            printf("\033[94m   => PREEMPT => processPtr=%p => new remain=%llu => preemptions=%llu\033[0m\n",
                   (void*)p,
                   (unsigned long long)p->remaining_time,
                   (unsigned long long)g_total_preempts);
            sim_sleep_scheduler(300000);

            if(g_current_alg == ALG_MLFQ) {
                p->mlfq_level++;
            }
            ready_queue_push(p);
        } else {
            p->end_time = now_sim;
            printf("\033[92m   => FINISH => processPtr=%p => total CPU used=%lu ms => time=%llu ms\033[0m\n",
                   (void*)p,
                   (unsigned long)slice,
                   (unsigned long long)os_time());
            sim_sleep_scheduler(300000);
        }
    }
    return NULL;
}

static void finalize_stats(void) {
    if(HPC_over_mode || g_list_count <= 0) {
        return;
    }
    for(int i=0; i<g_list_count; i++) {
        process_t* P = &g_process_list[i];
        uint64_t at = P->arrival_time;
        uint64_t st = P->first_response;
        uint64_t et = P->end_time;
        uint64_t bt = P->burst_time;

        uint64_t wait = (st > at) ? (st - at) : 0ULL;
        uint64_t tat  = (et > at) ? (et - at) : 0ULL;
        uint64_t resp = wait;

        g_total_wait += wait;
        g_total_tat  += tat;
        g_total_resp += resp;
        (void)bt; /* not always needed in this final calc. */
    }
    g_total_count = g_list_count;

    if(g_total_count > 0) {
        gAvgWait = (double)g_total_wait / (double)g_total_count;
        gAvgTAT  = (double)g_total_tat  / (double)g_total_count;
        gAvgResp = (double)g_total_resp / (double)g_total_count;
    }
    gPreemptions = g_total_preempts;
    gProcs       = g_total_count;
}

void scheduler_run(process_t* list, int count) {
    reset_accumulators();
    if(!list || count <= 0) return;

    if(g_current_alg == ALG_HPC_OVERSHADOW) {
        HPC_over_mode = 1;
        printf("\n\033[95m╔══════════════════════════════════════════════╗\n");
        printf("║       SCHEDULE NAME => HPC-OVERSHADOW        ║\n");
        printf("╚══════════════════════════════════════════════╝\033[0m\n");
        sim_sleep_scheduler(300000);

        os_run_hpc_overshadow();

        printf("\033[96m╔══════════════════════════════════════════════╗\n");
        printf("║  SCHEDULE END => HPC-OVERSHADOW => no stats  ║\n");
        printf("╚══════════════════════════════════════════════╝\033[0m\n");
        sim_sleep_scheduler(300000);
        return;
    }

    printf("\n\033[95m╔══════════════════════════════════════════════╗\n");
    printf("║   SCHEDULE NAME => %d (enum)                 ║\n", g_current_alg);
    printf("║   Number of processes => %d                  ║\n", count);
    printf("║   Time start => %llu ms                      ║\n", (unsigned long long)os_time());
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
    sim_sleep_scheduler(300000);

    switch(g_current_alg) {
    case ALG_BFS:
    case ALG_MLFQ:
        g_num_cores = 2;
        break;
    default:
        g_num_cores = 1;
        break;
    }

    ready_queue_init_policy(g_current_alg);

    g_process_list = list;
    g_list_count   = count;
    g_running      = 1;

    for(int i=0; i<count; i++) {
        ready_queue_push(&list[i]);
    }

    pthread_t tid[MAX_CORES];
    int n = (g_num_cores > MAX_CORES) ? MAX_CORES : g_num_cores;
    for(int i=0; i<n; i++) {
        pthread_create(&tid[i], NULL, core_thread_func, (void*)(long)i);
    }

    while(ready_queue_size() > 0) {
        sim_sleep_scheduler(200000);
    }
    g_running = 0;

    for(int i=0; i<n; i++) {
        ready_queue_push(NULL);
    }
    for(int i=0; i<n; i++) {
        pthread_join(tid[i], NULL);
    }

    ready_queue_destroy();
    finalize_stats();

    uint64_t total_time = get_global_sim_time();
    printf("\033[96m╔══════════════════════════════════════════════╗\n");
    printf("║ SCHEDULE END => alg=%d => totalTime=%llums    ║\n", g_current_alg, (unsigned long long)total_time);
    printf("║ Stats: preemptions=%llu, totalProcs=%llu     ║\n", (unsigned long long)gPreemptions, (unsigned long long)gProcs);
    printf("║ AvgWait=%.2f, AvgTAT=%.2f, AvgResp=%.2f       ║\n", gAvgWait, gAvgTAT, gAvgResp);
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
    sim_sleep_scheduler(300000);

    g_process_list = NULL;
    g_list_count   = 0;
}

void scheduler_fetch_report(sched_report_t* out) {
    if(!out) return;
    if(HPC_over_mode) {
        out->avg_wait       = 0.0;
        out->avg_turnaround = 0.0;
        out->avg_response   = 0.0;
        out->preemptions    = 0ULL;
        out->total_procs    = 0ULL;
    } else {
        out->avg_wait       = gAvgWait;
        out->avg_turnaround = gAvgTAT;
        out->avg_response   = gAvgResp;
        out->preemptions    = gPreemptions;
        out->total_procs    = gProcs;
    }
}
