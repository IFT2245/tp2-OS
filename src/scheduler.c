#include "scheduler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ready_queue.h"
#include "os.h"
#include "worker.h"

#define TIME_QUANTUM 2

/* Stats for scheduling run */
typedef struct {
    unsigned long      preemptions;
    unsigned long      total_processes;
    unsigned long long total_wait_time;
    unsigned long long total_turnaround_time;
    unsigned long long total_response_time;
} sched_stats_t;

static scheduler_alg_t current_alg = ALG_CFS;
static sched_stats_t   gSchedStats;
static int             gCount = 0;

typedef struct {
    uint64_t arrival_time;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t total_cpu_time;
    int      started;
} proc_info_t;

static proc_info_t* gProcInfo = NULL;

static const char* alg2str(scheduler_alg_t a){
    switch(a){
    case ALG_FIFO:          return "FIFO";
    case ALG_RR:            return "RR";
    case ALG_CFS:           return "CFS";
    case ALG_CFS_SRTF:      return "CFS-SRTF";
    case ALG_BFS:           return "BFS";
    case ALG_SJF:           return "SJF";
    case ALG_STRF:          return "STRF";
    case ALG_HRRN:          return "HRRN";
    case ALG_HRRN_RT:       return "HRRN-RT";
    case ALG_PRIORITY:      return "PRIORITY";
    case ALG_HPC_OVERSHADOW:return "HPC-OVER";
    case ALG_MLFQ:          return "MLFQ";
    default:                return "UNKNOWN";
    }
}

void scheduler_select_algorithm(scheduler_alg_t a){
    current_alg = a;
}

/* Recording arrivals, starts, CPU usage, etc. */
static void record_arrival(int i){
    if(!gProcInfo) return;
    if(gProcInfo[i].arrival_time == 0){
        gProcInfo[i].arrival_time = os_time();
    }
}
static void record_start(int i){
    if(!gProcInfo) return;
    if(!gProcInfo[i].started){
        gProcInfo[i].start_time = os_time();
        gProcInfo[i].started = 1;
    }
}
static void record_cpu_time(int i, unsigned long run){
    if(!gProcInfo) return;
    gProcInfo[i].total_cpu_time += run;
}
static void record_end(int i){
    if(!gProcInfo) return;
    if(gProcInfo[i].end_time == 0){
        gProcInfo[i].end_time = os_time();
    }
}

/* Finalize stats after scheduling loop finishes. */
static void finalize_stats(void){
    for(int i=0; i<gCount; i++){
        uint64_t at = gProcInfo[i].arrival_time;
        uint64_t st = gProcInfo[i].start_time;
        uint64_t et = gProcInfo[i].end_time;
        uint64_t cpu= gProcInfo[i].total_cpu_time;

        uint64_t turnaround = (et > at) ? (et - at) : 0;
        uint64_t response   = (st > at) ? (st - at) : 0;
        uint64_t wait       = (turnaround > cpu) ? (turnaround - cpu) : 0;

        gSchedStats.total_turnaround_time += turnaround;
        gSchedStats.total_response_time   += response;
        gSchedStats.total_wait_time       += wait;
    }
}

/* Helper to push all processes initially to queue. */
static void push_all(process_t* arr, int count){
    for(int i=0; i<count; i++){
        ready_queue_push(&arr[i]);
        gSchedStats.total_processes++;
        record_arrival(i);
    }
}

/* MLFQ variant: each partial run uses TIME_QUANTUM, re-queue at lower priority. */
static void run_mlfq(process_t* arr, int count){
    ready_queue_init_policy(ALG_MLFQ);
    push_all(arr, count);
    while(ready_queue_size() > 0){
        process_t* p = ready_queue_pop();
        if(!p) break;
        int idx = (int)(p - arr);
        record_start(idx);

        if(p->remaining_time > TIME_QUANTUM){
            simulate_process_partial(p, TIME_QUANTUM);
            record_cpu_time(idx, TIME_QUANTUM);
            p->remaining_time -= TIME_QUANTUM;
            gSchedStats.preemptions++;
            p->mlfq_level++;
            ready_queue_push(p);
        } else {
            simulate_process_partial(p, p->remaining_time);
            record_cpu_time(idx, p->remaining_time);
            p->remaining_time = 0;
            record_end(idx);
        }
    }
    ready_queue_destroy();
}

/* BFS: simple RR with TIME_QUANTUM. */
static void run_bfs(process_t* arr, int count){
    ready_queue_init_policy(ALG_BFS);
    push_all(arr, count);
    while(ready_queue_size() > 0){
        process_t* p = ready_queue_pop();
        if(!p) break;
        int idx = (int)(p - arr);
        record_start(idx);

        if(p->remaining_time > TIME_QUANTUM){
            simulate_process_partial(p, TIME_QUANTUM);
            record_cpu_time(idx, TIME_QUANTUM);
            p->remaining_time -= TIME_QUANTUM;
            gSchedStats.preemptions++;
            ready_queue_push(p);
        } else {
            simulate_process_partial(p, p->remaining_time);
            record_cpu_time(idx, p->remaining_time);
            p->remaining_time = 0;
            record_end(idx);
        }
    }
    ready_queue_destroy();
}

/* Preemptive scheduling for RR, CFS-SRTF, STRF, HRRN-RT */
static void run_preemptive(process_t* arr, int count){
    ready_queue_init_policy(current_alg);
    push_all(arr, count);
    while(ready_queue_size() > 0){
        process_t* p = ready_queue_pop();
        if(!p) break;
        int idx = (int)(p - arr);
        record_start(idx);

        if(p->remaining_time > TIME_QUANTUM){
            simulate_process_partial(p, TIME_QUANTUM);
            record_cpu_time(idx, TIME_QUANTUM);
            p->remaining_time -= TIME_QUANTUM;
            if(current_alg == ALG_CFS_SRTF) {
                p->vruntime += TIME_QUANTUM;
            }
            gSchedStats.preemptions++;
            ready_queue_push(p);
        } else {
            simulate_process_partial(p, p->remaining_time);
            record_cpu_time(idx, p->remaining_time);
            p->remaining_time = 0;
            record_end(idx);
        }
    }
    ready_queue_destroy();
}

/* Non-preemptive scheduling for FIFO, CFS, SJF, HRRN, PRIORITY, etc. */
static void run_non_preemptive(process_t* arr, int count){
    ready_queue_init_policy(current_alg);
    push_all(arr, count);
    while(ready_queue_size() > 0){
        process_t* p = ready_queue_pop();
        if(!p) break;
        int idx = (int)(p - arr);
        record_start(idx);
        simulate_process(p);
        record_cpu_time(idx, p->remaining_time);
        p->remaining_time = 0;
        record_end(idx);
    }
    ready_queue_destroy();
}

void scheduler_run(process_t* list, int count){
    if(!list || count <= 0) return;
    memset(&gSchedStats, 0, sizeof(gSchedStats));
    gCount = count;

    /* For extended stats */
    gProcInfo = (proc_info_t*)calloc((size_t)count, sizeof(proc_info_t));
    if(!gProcInfo){
        fprintf(stderr,"[scheduler_run] Not enough memory.\n");
        return;
    }

    if(current_alg == ALG_HPC_OVERSHADOW){
        /* Just run overshadow operation, no real scheduling needed. */
        os_run_hpc_overshadow();
        free(gProcInfo);
        gProcInfo = NULL;
        gCount = 0;
        return;
    }

    uint64_t t0 = os_time();

    switch(current_alg){
    case ALG_MLFQ:
        run_mlfq(list, count);
        break;
    case ALG_BFS:
        run_bfs(list, count);
        break;
    case ALG_RR:
    case ALG_CFS_SRTF:
    case ALG_STRF:
    case ALG_HRRN_RT:
        run_preemptive(list, count);
        break;
    default:
        /* Non-preemptive (FIFO, CFS, SJF, HRRN, PRIORITY) */
        run_non_preemptive(list, count);
        break;
    }

    uint64_t total_time = os_time() - t0;
    finalize_stats();

    printf("\033[92mStats for %s: total_time=%llu ms\033[0m\n", alg2str(current_alg), (unsigned long long)total_time);
    printf("Scheduler Stats => processes: %lu, preemptions: %lu\n",
           gSchedStats.total_processes, gSchedStats.preemptions);
    if(gSchedStats.total_processes > 0){
        double n = (double)gSchedStats.total_processes;
        double avg_wait  = (double)gSchedStats.total_wait_time       / n;
        double avg_tat   = (double)gSchedStats.total_turnaround_time / n;
        double avg_resp  = (double)gSchedStats.total_response_time   / n;
        printf("\033[94m┌─────────────────────────────────────────┐\n");
        printf("│   Wait=%.2f ms  TAT=%.2f ms  RESP=%.2f ms │\n", avg_wait, avg_tat, avg_resp);
        printf("└─────────────────────────────────────────┘\033[0m\n");
    }
    free(gProcInfo);
    gProcInfo = NULL;
    gCount = 0;
    fflush(stdout);
}
