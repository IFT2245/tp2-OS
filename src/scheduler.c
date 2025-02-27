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

/* We assume up to 4 cores max for BFS/MLFQ. */
#ifndef MAX_CORES
#define MAX_CORES 4
#endif

/* ---------- Global scheduling states ---------- */
static scheduler_alg_t  g_current_alg   = ALG_CFS;
static int              g_num_cores     = 1;
static int              g_running       = 0;

/*
   The global simulated time in "ms" (not real ms, but a time-step
   that increments with each partial slice).
*/
static uint64_t         g_sim_time      = 0;
static pthread_mutex_t  g_sim_time_lock = PTHREAD_MUTEX_INITIALIZER;

/* BFS dynamic quantum (for reference in BFS tests). */
static unsigned long    g_bfs_quantum   = 4;

/* Data about processes and arrivals. */
static process_t*       g_process_list          = NULL;
static int              g_list_count            = 0;

/* We’ll keep a second array sorted by arrival_time. */
static process_t*       g_process_list_sorted   = NULL;
static int              g_next_arrival_index    = 0;

/* Count how many are finished. Once finished == g_list_count => end scheduling. */
static int              g_finished_count        = 0;

/* Protect arrival feeding & finished_count updates with a small lock. */
static pthread_mutex_t  g_arrivals_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  g_finish_lock   = PTHREAD_MUTEX_INITIALIZER;

/*
   schedule_stats_t tracks final stats after normal scheduling runs.
   HPC overshadow / HPC overlay skip normal stats.
*/
static schedule_stats_t g_stats;

/* Provide BFS quantum to BFS tests, as originally done. */
unsigned long scheduler_get_bfs_quantum(void) {
    return g_bfs_quantum;
}

/* Access global sim time (used by ready_queue, e.g. HRRN, MLFQ). */
uint64_t get_global_sim_time(void) {
    pthread_mutex_lock(&g_sim_time_lock);
    uint64_t t = g_sim_time;
    pthread_mutex_unlock(&g_sim_time_lock);
    return t;
}

/* Reset scheduling stats & time at run start. */
static void reset_schedule_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
    g_sim_time       = 0;
    g_finished_count = 0;
    g_next_arrival_index = 0;
}

/* ---------- Utility: dynamic quantum for preemptive algs ---------- */
static unsigned long get_dynamic_quantum(scheduler_alg_t alg) {
    size_t rq_size = ready_queue_size();
    switch(alg) {
        case ALG_RR: {
            /* Example: baseQuantum=2, plus a small adjustment if queue is large. */
            const unsigned long baseQuantum = 2;
            unsigned long dynamic_part = (unsigned long)(rq_size / 2);
            return baseQuantum + dynamic_part;
        }
        case ALG_BFS:
            /*
               BFS => choose base slice=4 + maybe +1 if queue>5.
               Store in g_bfs_quantum for BFS tests.
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
            /* Non-preemptive => run to completion => quantum=0 => no forced preemption. */
            return 0;
    }
}

/* ---------- Feed newly arrived processes up to current sim_time ---------- */
static void feed_arrivals_up_to_time(uint64_t now_sim) {
    pthread_mutex_lock(&g_arrivals_lock);
    /*
      If the next arrival in g_process_list_sorted has arrival_time <= now_sim,
      push it into the ready_queue. We continue while more processes are also ready.
    */
    while (g_next_arrival_index < g_list_count) {
        process_t* arrP = &g_process_list_sorted[g_next_arrival_index];
        if (arrP->arrival_time <= now_sim) {
            ready_queue_push(arrP);
            g_next_arrival_index++;
        } else {
            /* The next process has not arrived yet, so break. */
            break;
        }
    }
    pthread_mutex_unlock(&g_arrivals_lock);
}

/* ---------- Mark a process as finished, check if we can end scheduling ---------- */
static void mark_process_finished(void) {
    pthread_mutex_lock(&g_finish_lock);
    g_finished_count++;
    /* If all processes are done => push sentinel for each core => threads will exit. */
    if (g_finished_count >= g_list_count && g_running) {
        g_running = 0;  /* stop any further loops */
        /* cause each core to pop a sentinel => break out. */
        int n = (g_num_cores > MAX_CORES) ? MAX_CORES : g_num_cores;
        for (int i = 0; i < n; i++) {
            ready_queue_push(NULL);
        }
    }
    pthread_mutex_unlock(&g_finish_lock);
}

/*
   HPC overshadow or HPC overlay => special one-shot mode.
   We skip normal stats because no “processes” are run in the usual sense.
*/
static void run_hpc_mode(int overshadow) {
    if (stats_get_speed_mode() == 0) {
        if (overshadow) {
            printf("\n\033[95m[HPC OVERSHADOW MODE - Start]\033[0m\n");
            usleep(300000);
        } else {
            printf("\n\033[95m[HPC OVERLAY MODE - Start]\033[0m\n");
            usleep(300000);
        }
    }
    if (overshadow) {
        g_stats.HPC_over_mode = 1;
        os_run_hpc_overshadow();
    } else {
        g_stats.HPC_overlay_mode = 1;
        os_run_hpc_overlay();
    }
}

/* ---------- The actual scheduling loop for each core ---------- */
static void* core_thread_func(void* arg) {
    long core_id = (long)arg;

    while (1) {
        /* Pop next process from the ready_queue. If sentinel => exit. */
        process_t* p = ready_queue_pop();
        if (!p) {
            /* This is either sentinel or empty queue. If g_running=1, we keep going.
               But if we got a sentinel, that means we are done.
            */
            if (!g_running) {
                /* actual sentinel => time to exit. */
                break;
            } else {
                /* Could happen if queue is empty but not all processes arrived.
                   Just loop again (or sleep briefly).
                */
                usleep(10000);
                continue;
            }
        }

        /* If first time responding => store response time. */
        if (!p->responded) {
            p->responded = 1;
            p->first_response = get_global_sim_time();
        }
        p->times_owning_core++;

        /* Decide time-slice (quantum) for preemptive scheduling. */
        unsigned long q = get_dynamic_quantum(g_current_alg);
        int preemptive   = (q > 0) ? 1 : 0;

        /* slice = min(q, remaining_time) or entire remain if non-preemptive. */
        uint64_t remain = p->remaining_time;
        unsigned long slice = preemptive ?
                              ( (remain > q) ? q : (unsigned long)remain ) :
                              (unsigned long)remain;

        /* "Run" that partial slice. */
        uint64_t real_t_before = os_time();
        if (stats_get_speed_mode() == 0) {
            printf("\033[93m[time=%llu ms] => container=1 core=%ld => scheduling process=%p\n"
                   "   => burst_time=%lu, prio=%d, vruntime=%llu, remain=%llu, timesScheduled=%d\n\033[0m",
                   (unsigned long long)real_t_before,
                   core_id, (void*)p,
                   (unsigned long)p->burst_time,
                   p->priority,
                   (unsigned long long)p->vruntime,
                   (unsigned long long)p->remaining_time,
                   p->times_owning_core);
            usleep(300000);
        }

        simulate_process_partial(p, slice, (int)core_id);

        /* update remain, global sim_time, check new arrivals. */
        pthread_mutex_lock(&g_sim_time_lock);
        p->remaining_time -= slice;

        g_sim_time += slice; /* advance simulated time by the slice. */
        uint64_t now_sim = g_sim_time;
        pthread_mutex_unlock(&g_sim_time_lock);

        /* If using CFS or CFS-SRTF => add to vruntime. */
        if (g_current_alg == ALG_CFS || g_current_alg == ALG_CFS_SRTF) {
            p->vruntime += slice;
        }

        /* Feed newly arrived processes if any. */
        feed_arrivals_up_to_time(now_sim);

        /* If process is not finished => requeue if preemptive => preempt++ */
        if (p->remaining_time > 0) {
            if (preemptive) {
                __sync_fetch_and_add(&g_stats.total_preempts, 1ULL);

                if (stats_get_speed_mode() == 0) {
                    printf("\033[94m   => PREEMPT => process=%p => new remain=%llu => preemptions=%llu\n\033[0m",
                           (void*)p,
                           (unsigned long long)p->remaining_time,
                           (unsigned long long)g_stats.total_preempts);
                    usleep(300000);
                }
                /* degrade to next queue if MLFQ, etc. */
                if (g_current_alg == ALG_MLFQ) {
                    p->mlfq_level++;
                }
                ready_queue_push(p);
            } else {
                /* non-preemptive => it should run to completion, so no requeue. */
                /* But in practice, we've just used the entire remain. So it's done. */
                p->end_time = now_sim;
                mark_process_finished();

                if (stats_get_speed_mode() == 0) {
                    printf("\033[92m   => FINISH => process=%p => total CPU used=%lu => time=%llu ms\n\033[0m",
                           (void*)p, slice, (unsigned long long)os_time());
                    usleep(300000);
                }
            }
        } else {
            /* remain_time <= 0 => done. */
            p->end_time = now_sim;
            mark_process_finished();

            if (stats_get_speed_mode() == 0) {
                printf("\033[92m   => FINISH => process=%p => total CPU used=%lu => time=%llu ms\n\033[0m",
                       (void*)p, slice, (unsigned long long)os_time());
                usleep(300000);
            }
        }
    }
    return NULL;
}

/* ---------- Helpers to compute final stats ---------- */
static void finalize_stats(void) {
    for (int i = 0; i < g_list_count; i++) {
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

    if (g_stats.total_count > 0) {
        g_stats.avg_wait       = (double)g_stats.total_wait / (double)g_stats.total_count;
        g_stats.avg_turnaround = (double)g_stats.total_tat  / (double)g_stats.total_count;
        g_stats.avg_response   = (double)g_stats.total_resp / (double)g_stats.total_count;
    }
    g_stats.preemptions  = g_stats.total_preempts;
    g_stats.total_procs   = g_stats.total_count;
}

/* ---------- Public / Exposed Functions ---------- */

void scheduler_select_algorithm(scheduler_alg_t a) {
    g_current_alg = a;
}

void scheduler_run(process_t* list, int count) {
    reset_schedule_stats();
    if(!list || count <= 0) return;

    /* HPC overshadow/overlay => skip normal scheduling. */
    if (g_current_alg == ALG_HPC_OVERSHADOW) {
        run_hpc_mode(/*overshadow=*/1);
        return;
    }
    if (g_current_alg == ALG_HPC_OVERLAY) {
        run_hpc_mode(/*overshadow=*/0);
        return;
    }

    /* BFS or MLFQ => we use 2 cores, else 1. */
    if (g_current_alg == ALG_BFS || g_current_alg == ALG_MLFQ) {
        g_num_cores = 2;
    } else {
        g_num_cores = 1;
    }

    /* Keep a copy of the processes so we can do final stats. */
    g_process_list = list;
    g_list_count   = count;

    /* Also build a second array sorted by arrival_time. */
    g_process_list_sorted = (process_t*)calloc((size_t)count, sizeof(process_t));
    memcpy(g_process_list_sorted, list, count * sizeof(process_t));
    /* Simple ascending sort by arrival_time: */
    for(int i=0; i<count-1; i++){
        for(int j=i+1; j<count; j++){
            if(g_process_list_sorted[j].arrival_time < g_process_list_sorted[i].arrival_time) {
                process_t tmp = g_process_list_sorted[i];
                g_process_list_sorted[i] = g_process_list_sorted[j];
                g_process_list_sorted[j] = tmp;
            }
        }
    }

    if (stats_get_speed_mode() == 0) {
        printf("\n\033[95m╔══════════════════════════════════════════════╗\n");
        printf(       "║ SCHEDULE => alg=%d => #processes=%d            \n", g_current_alg, count);
        printf(       "║ RealTime start => %llu ms                     \n", (unsigned long long)os_time());
        printf(       "╚══════════════════════════════════════════════╝\033[0m\n");
        usleep(300000);
    }

    /* Initialize the ready_queue with the chosen alg. */
    ready_queue_init_policy(g_current_alg);

    /* Initially, no processes are in the queue. We feed any that have arrival_time=0. */
    g_running = 1;
    feed_arrivals_up_to_time(0);

    /* Spawn core threads. */
    pthread_t tid[MAX_CORES];
    int n = (g_num_cores > MAX_CORES) ? MAX_CORES : g_num_cores;
    for (int i=0; i<n; i++) {
        pthread_create(&tid[i], NULL, core_thread_func, (void*)(long)i);
    }

    /*
       Wait until all processes finish.
       We do that by sleeping periodically until g_finished_count == g_list_count.
       Another approach is a condition variable. We'll keep it simple.
    */
    while (1) {
        pthread_mutex_lock(&g_finish_lock);
        if (g_finished_count >= g_list_count) {
            pthread_mutex_unlock(&g_finish_lock);
            break;
        }
        pthread_mutex_unlock(&g_finish_lock);
        usleep(50000); /* poll period */
    }

    /* Now push sentinel to ensure threads exit if not already done. */
    g_running = 0;
    for(int i=0; i<n; i++){
        ready_queue_push(NULL);
    }
    for(int i=0; i<n; i++){
        pthread_join(tid[i], NULL);
    }

    /* done => destroy the queue, finalize stats. */
    ready_queue_destroy();
    finalize_stats();

    if (stats_get_speed_mode()==0) {
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

    free(g_process_list_sorted);
    g_process_list_sorted = NULL;
    g_process_list        = NULL;
    g_list_count          = 0;
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
