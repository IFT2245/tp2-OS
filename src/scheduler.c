#include "scheduler.h"
#include "ready_queue.h"
#include "os.h"
#include "worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIME_QUANTUM 2

static scheduler_alg_t current_alg = ALG_CFS;

typedef struct {
    unsigned long preemptions;
    unsigned long total_processes;
    unsigned long long total_wait;
    unsigned long long total_turnaround;
    unsigned long long total_response;
} sched_stats_t;

static sched_stats_t gSchedStats;
static int gCount=0;

typedef struct {
    uint64_t arrival_time;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t total_cpu_time;
    int started;
} proc_info_t;

static proc_info_t* gInfo=NULL;

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

static void record_arrival(int i){
    if(!gInfo) return;
    if(gInfo[i].arrival_time == 0) gInfo[i].arrival_time = os_time();
}
static void record_start(int i){
    if(!gInfo) return;
    if(!gInfo[i].started){
        gInfo[i].start_time = os_time();
        gInfo[i].started    = 1;
    }
}
static void record_cpu_time(int i, unsigned long slice){
    if(gInfo) gInfo[i].total_cpu_time += slice;
}
static void record_end(int i){
    if(!gInfo) return;
    if(gInfo[i].end_time == 0) gInfo[i].end_time = os_time();
}
static void finalize_stats(void){
    for(int i=0;i<gCount;i++){
        uint64_t at   = gInfo[i].arrival_time;
        uint64_t st   = gInfo[i].start_time;
        uint64_t et   = gInfo[i].end_time;
        uint64_t cpu  = gInfo[i].total_cpu_time;
        uint64_t turn = (et>at)?(et - at):0;
        uint64_t resp = (st>at)?(st - at):0;
        uint64_t wait = (turn>cpu)?(turn - cpu):0;
        gSchedStats.total_turnaround += turn;
        gSchedStats.total_response   += resp;
        gSchedStats.total_wait       += wait;
    }
}

static void push_all(process_t* arr, int count){
    for(int i=0;i<count;i++){
        ready_queue_push(&arr[i]);
        gSchedStats.total_processes++;
        record_arrival(i);
    }
}

static void run_mlfq(process_t* arr,int count){
    ready_queue_init_policy(ALG_MLFQ);
    push_all(arr,count);
    while(ready_queue_size()>0){
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

static void run_bfs(process_t* arr,int count){
    ready_queue_init_policy(ALG_BFS);
    push_all(arr,count);
    while(ready_queue_size()>0){
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

static void run_preemptive(process_t* arr,int count){
    ready_queue_init_policy(current_alg);
    push_all(arr,count);
    while(ready_queue_size()>0){
        process_t* p = ready_queue_pop();
        if(!p) break;
        int idx = (int)(p - arr);
        record_start(idx);
        if(p->remaining_time > TIME_QUANTUM){
            simulate_process_partial(p, TIME_QUANTUM);
            record_cpu_time(idx, TIME_QUANTUM);
            p->remaining_time -= TIME_QUANTUM;
            if(current_alg == ALG_CFS_SRTF) p->vruntime += TIME_QUANTUM;
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

static void run_non_preemptive(process_t* arr,int count){
    ready_queue_init_policy(current_alg);
    push_all(arr,count);
    while(ready_queue_size()>0){
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
    if(!list || count<=0) return;
    memset(&gSchedStats, 0, sizeof(gSchedStats));
    gCount = count;
    gInfo  = (proc_info_t*)calloc((size_t)count, sizeof(proc_info_t));
    if(!gInfo) return;

    if(current_alg == ALG_HPC_OVERSHADOW){
        os_run_hpc_overshadow();
        free(gInfo);
        gInfo = NULL;
        gCount=0;
        return;
    }
    uint64_t t0 = os_time();
    switch(current_alg){
    case ALG_MLFQ: run_mlfq(list, count);       break;
    case ALG_BFS:  run_bfs(list, count);        break;
    case ALG_RR:
    case ALG_CFS_SRTF:
    case ALG_STRF:
    case ALG_HRRN_RT:
        run_preemptive(list, count);
        break;
    default:
        run_non_preemptive(list, count);
        break;
    }
    uint64_t total_time = os_time() - t0;
    finalize_stats();

    printf("\033[92m[Scheduler] %s: total_time=%llu ms\n\033[0m", alg2str(current_alg),(unsigned long long)total_time);
    printf("[Scheduler] processes:%lu preemptions:%lu\n", gSchedStats.total_processes, gSchedStats.preemptions);
    if(gSchedStats.total_processes > 0){
        double n = (double)gSchedStats.total_processes;
        double avg_wait = gSchedStats.total_wait / n;
        double avg_tat  = gSchedStats.total_turnaround / n;
        double avg_resp = gSchedStats.total_response / n;
        printf("[Scheduler] Wait=%.2f TAT=%.2f RESP=%.2f\n", avg_wait, avg_tat, avg_resp);
    }
    free(gInfo);
    gInfo = NULL;
    gCount=0;
}
