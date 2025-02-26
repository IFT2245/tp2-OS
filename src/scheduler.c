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
   The global simulated time in "ms" (not real milliseconds, but a
   time-step increment for each partial slice).
*/
static uint64_t         g_sim_time      = 0;
static pthread_mutex_t  g_sim_time_lock = PTHREAD_MUTEX_INITIALIZER;

/* BFS dynamic quantum (for BFS tests). */
static unsigned long    g_bfs_quantum   = 4;

/* Arrays and counters for processes. */
static process_t*       g_process_list         = NULL;
static int              g_list_count           = 0;

/* Array of pointers sorted by arrival_time, so we can feed arrivals in order. */
static process_t**      g_proc_sorted_ptrs     = NULL;
static int              g_next_arrival_index   = 0;

/* Count how many have finished. Once finished == g_list_count => end scheduling. */
static int              g_finished_count       = 0;

/* Protect arrival feeding & finish counting. */
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
    switch (alg) {
        case ALG_RR: {
            /* Example: baseQuantum=2 plus a small adjustment if queue is large. */
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
            /* short fixed slice => 2 ms */
            return 2;

        default:
            /* Non-preemptive => run to completion => quantum=0 => no forced preempt. */
            return 0;
    }
}

/* ---------- Feed newly arrived processes up to current sim_time ---------- */
static void feed_arrivals_up_to_time(uint64_t now_sim) {
    pthread_mutex_lock(&g_arrivals_lock);
    /*
      We check the next arrival in the sorted pointer array. If its arrival_time
      <= now_sim, push it into the ready_queue. Continue while more processes
      are also ready.
    */
    while (g_next_arrival_index < g_list_count) {
        process_t* arrP = g_proc_sorted_ptrs[g_next_arrival_index];
        if (arrP->arrival_time <= now_sim) {
            ready_queue_push(arrP);
            g_next_arrival_index++;
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&g_arrivals_lock);
}

/* ---------- Mark a process as finished ---------- */
static void mark_process_finished(void) {
    pthread_mutex_lock(&g_finish_lock);
    g_finished_count++;
    /* If all processes done => push sentinel for each core => threads exit. */
    if (g_finished_count >= g_list_count && g_running) {
        g_running = 0;  /* stop further loops */
        int n = (g_num_cores > MAX_CORES) ? MAX_CORES : g_num_cores;
        for (int i = 0; i < n; i++) {
            ready_queue_push(NULL); /* sentinel */
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

/* ---------- The scheduling loop for each core ---------- */
static void* core_thread_func(void* arg) {
    long core_id = (long)arg;

    while (1) {
        if (os_concurrency_stop_requested()) {
            break;
        }
        /* Pop next process. If sentinel => exit if g_running=0. */
        process_t* p = ready_queue_pop();
        if (!p) {
            if (!g_running) {
                /* Actual sentinel => time to exit. */
                break;
            } else {
                /* Queue empty but not done => try again. */
                usleep(10000);
                continue;
            }
        }

        /* If first time responding => record response time. */
        if (!p->responded) {
            p->responded = 1;
            p->first_response = get_global_sim_time();
        }
        p->times_owning_core++;

        /* Decide time-slice (quantum). */
        unsigned long q = get_dynamic_quantum(g_current_alg);
        int preemptive   = (q > 0) ? 1 : 0;

        /* slice = min(q, p->remaining_time) if preemptive, else entire remain. */
        uint64_t remain = p->remaining_time;
        unsigned long slice = preemptive
                              ? ((remain > q) ? q : (unsigned long)remain)
                              : (unsigned long)remain;

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

        /* Update remain, global sim_time, arrivals, finishing. */
        pthread_mutex_lock(&g_sim_time_lock);
        p->remaining_time -= slice;
        g_sim_time        += slice; /* advance simulation time by slice. */
        uint64_t now_sim   = g_sim_time;
        pthread_mutex_unlock(&g_sim_time_lock);

        /* If CFS or CFS-SRTF => increment vruntime. */
        if (g_current_alg == ALG_CFS || g_current_alg == ALG_CFS_SRTF) {
            p->vruntime += slice;
        }

        /* Feed newly arrived processes if any. */
        feed_arrivals_up_to_time(now_sim);

        /* Check if finished or preempt. */
        if (p->remaining_time > 0) {
            /* Not done => requeue if preemptive => count preemptions. */
            if (preemptive) {
                __sync_fetch_and_add(&g_stats.total_preempts, 1ULL);

                if (stats_get_speed_mode() == 0) {
                    printf("\033[94m   => PREEMPT => process=%p => new remain=%llu => preemptions=%llu\n\033[0m",
                           (void*)p,
                           (unsigned long long)p->remaining_time,
                           (unsigned long long)g_stats.total_preempts);
                    usleep(300000);
                }

                /* For MLFQ, degrade queue level. */
                if (g_current_alg == ALG_MLFQ) {
                    p->mlfq_level++;
                }
                ready_queue_push(p);
            } else {
                /* Non-preemptive => must have run to completion. Actually, if slice=remain, it's done. */
                /* But let's be safe: no requeue. Mark finished. */
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

/*
   finalize_stats => compute wait/turnaround/response from each original process's
   arrival_time, first_response, end_time, etc.
*/
static void finalize_stats(void) {
    for (int i = 0; i < g_list_count; i++) {
        process_t* P = &g_process_list[i];
        uint64_t at  = P->arrival_time;
        uint64_t st  = P->first_response;
        uint64_t et  = P->end_time;

        uint64_t wait = 0ULL;
        uint64_t tat  = 0ULL;
        uint64_t resp = 0ULL;

        if (st > at) {
            wait = st - at;
        }
        if (et > at) {
            tat = et - at;
        }
        resp = wait; /* in this simulation, response time = time until first scheduled */

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
    g_stats.preemptions = g_stats.total_preempts;
    g_stats.total_procs  = g_stats.total_count;
}

/* ---------- Public / Exposed Functions ---------- */

void scheduler_select_algorithm(scheduler_alg_t a) {
    g_current_alg = a;
}

void scheduler_run(process_t* list, int count) {
    reset_schedule_stats();
    if (!list || count <= 0) return;

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

    /* Keep pointer to original array so finalize_stats can read from it. */
    g_process_list = list;
    g_list_count   = count;

    /* Build an array of pointers sorted by arrival_time. */
    g_proc_sorted_ptrs = (process_t**)calloc((size_t)count, sizeof(process_t*));
    for (int i = 0; i < count; i++) {
        g_proc_sorted_ptrs[i] = &list[i];
    }
    /* Sort them by arrival_time ascending. */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (g_proc_sorted_ptrs[j]->arrival_time < g_proc_sorted_ptrs[i]->arrival_time) {
                process_t* tmp                = g_proc_sorted_ptrs[i];
                g_proc_sorted_ptrs[i]         = g_proc_sorted_ptrs[j];
                g_proc_sorted_ptrs[j]         = tmp;
            }
        }
    }

    /* Show scheduling header. */
    if (stats_get_speed_mode() == 0) {
        printf("\n\033[95m╔══════════════════════════════════════════════╗\n");
        printf(      "║ SCHEDULE => alg=%d => #processes=%d            \n", g_current_alg, count);
        printf(      "║ RealTime start => %llu ms                     \n", (unsigned long long)os_time());
        printf(      "╚══════════════════════════════════════════════╝\033[0m\n");
        usleep(300000);
    }

    /* Initialize the ready_queue with the chosen algorithm. */
    ready_queue_init_policy(g_current_alg);

    /* We start scheduling => set g_running=1, feed arrivals with arrival_time=0. */
    g_running = 1;
    feed_arrivals_up_to_time(0);

    /* Spawn core threads. */
    pthread_t tid[MAX_CORES];
    int n = (g_num_cores > MAX_CORES) ? MAX_CORES : g_num_cores;
    for (int i = 0; i < n; i++) {
        pthread_create(&tid[i], NULL, core_thread_func, (void*)(long)i);
    }

    /* Wait until all processes finish or concurrency stop is requested. */
    while (1) {
        pthread_mutex_lock(&g_finish_lock);
        if (g_finished_count >= g_list_count) {
            pthread_mutex_unlock(&g_finish_lock);
            break;
        }
        pthread_mutex_unlock(&g_finish_lock);

        if (os_concurrency_stop_requested()) {
            /* If user sends SIGTERM => break out early. */
            break;
        }
        usleep(50000); /* poll period */
    }

    /* Now push sentinel to ensure threads exit if not already done. */
    g_running = 0;
    for (int i = 0; i < n; i++) {
        ready_queue_push(NULL);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(tid[i], NULL);
    }

    /* done => destroy queue, finalize stats. */
    ready_queue_destroy();
    finalize_stats();

    /* Print final stats. */
    if (stats_get_speed_mode() == 0) {
        uint64_t total_time = get_global_sim_time();
        printf("\033[96m╔══════════════════════════════════════════════╗\n");
        printf(      "║ SCHEDULE END => alg=%d => totalTime=%llums    \n",
                     g_current_alg, (unsigned long long)total_time);
        printf(      "║ Stats: preemptions=%llu, totalProcs=%llu     \n",
                     (unsigned long long)g_stats.total_preempts,
                     (unsigned long long)g_stats.total_procs);
        printf(      "║ AvgWait=%.2f, AvgTAT=%.2f, AvgResp=%.2f       \n",
                     g_stats.avg_wait, g_stats.avg_turnaround, g_stats.avg_response);
        printf(      "╚══════════════════════════════════════════════╝\033[0m\n");
        usleep(300000);
    }

    /* Free the pointer array. */
    if (g_proc_sorted_ptrs) {
        free(g_proc_sorted_ptrs);
        g_proc_sorted_ptrs = NULL;
    }
    g_list_count = 0;
    g_process_list = NULL;
}

void scheduler_fetch_report(sched_report_t* out) {
    if (!out) return;

    /* HPC overshadow/overlay => zero normal stats. */
    if (g_stats.HPC_over_mode || g_stats.HPC_overlay_mode) {
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
