#include "scheduler.h"
#include "ready_queue.h"
#include "os.h"
#include "worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* We allow up to some max cores. */
#ifndef MAX_CORES
#define MAX_CORES 4
#endif

static scheduler_alg_t current_alg=ALG_CFS;

/* We'll maintain a global simulated_time, incremented by slices. */
static uint64_t g_sim_time=0;

/* Because we do concurrency, we need a lock to safely read/update g_sim_time. */
static pthread_mutex_t sim_time_lock = PTHREAD_MUTEX_INITIALIZER;

/* Accessor for the ready_queue to see the "now" for HRRN, etc. */
uint64_t get_global_sim_time(void){
    pthread_mutex_lock(&sim_time_lock);
    uint64_t t = g_sim_time;
    pthread_mutex_unlock(&sim_time_lock);
    return t;
}

typedef struct {
    double avg_wait;
    double avg_turnaround;
    double avg_response;
    unsigned long long preemptions;
    unsigned long long total_procs;
    uint64_t total_wait;
    int total_processes;
    uint64_t total_response;
    int total_turnaround;
} sched_stats_accum_t;

static sched_stats_accum_t gStats;
static double gAvgWait=0.0, gAvgTAT=0.0, gAvgResp=0.0;
static unsigned long long gPreemptions=0, gProcs=0;
static int HPC_over_mode=0;

/* Track whether we are done scheduling or not. If no processes remain, we end. */
static int g_running=0;
static int g_num_cores=1;

static void reset_accumulators(void){
    memset(&gStats, 0, sizeof(gStats));
    gAvgWait=0; gAvgTAT=0; gAvgResp=0;
    gPreemptions=0; gProcs=0;
    HPC_over_mode=0;
    g_sim_time=0;
}

/* Worker thread for each core => pop from ready queue, run partial or full. */
static void* scheduling_core_thread(void* arg){
    long core_id = (long)arg;
    int quantum=2; /* for RR-like partial or BFS, etc. */

    while(g_running){
        process_t* p = ready_queue_pop(); /* block until available or done */

        /* Check if we should exit because the queue was destroyed. */
        if(!g_running || !p) break;

        /* Print line for scheduling (real time for user, but we also have sim_time inside). */
        uint64_t real_t = os_time(); /* for user display only */
        printf("\033[93m[time=%llu ms] => container=1 core=%ld => scheduling processPtr=%p\n"
               "   => burst_time=%lu, prio=%d, vruntime=%llu, remain=%llu, timesScheduled=%d\033[0m\n",
               (unsigned long long)real_t,
               core_id, (void*)p,
               (unsigned long)p->burst_time,
               p->priority,
               (unsigned long long)p->vruntime,
               (unsigned long long)p->remaining_time,
               p->times_owning_core);
        usleep(300000);

        /* Mark first response time if not responded yet. */
        if(!p->responded){
            p->responded=1;
            p->first_response = get_global_sim_time();
        }
        p->times_owning_core++;

        /* Decide how many ms to run (in simulated time). */
        unsigned long slice = 0;
        int preemptive = 0;

        switch(current_alg){
        case ALG_RR:
        case ALG_BFS:
        case ALG_CFS_SRTF:
        case ALG_STRF:
        case ALG_HRRN_RT:
        case ALG_MLFQ:
            preemptive = 1; /* partial preemption possible */
            break;
        default:
            preemptive = 0; /* run entire burst with no preemption */
            break;
        }

        if(preemptive){
            if(p->remaining_time > (unsigned long)quantum){
                slice = quantum;
            } else {
                slice = p->remaining_time;
            }
        } else {
            slice = p->remaining_time;
        }

        /* Actually "run" the slice in simulated time. */
        simulate_process_partial(p, slice); /* real-time sleep for user */
        /* Then increment sim_time. */
        pthread_mutex_lock(&sim_time_lock);
        g_sim_time += slice;
        pthread_mutex_unlock(&sim_time_lock);

        /* Update stats in p. */
        p->remaining_time -= slice;
        if(current_alg==ALG_CFS_SRTF){
            p->vruntime += slice;
        }

        if(preemptive && p->remaining_time>0){
            /* we preempt => put back in queue => increment preemptions. */
            __sync_fetch_and_add(&gStats.preemptions, 1ULL);

            printf("\033[94m   => PREEMPT => processPtr=%p => new remain=%llu => preemptions=%llu\033[0m\n",
                   (void*)p,
                   (unsigned long long)p->remaining_time,
                   (unsigned long long)gStats.preemptions);
            usleep(300000);

            /* If MLFQ, we move it to next queue. */
            if(current_alg==ALG_MLFQ){
                p->mlfq_level++;
            }
            ready_queue_push(p);
        } else {
            /* finished. */
            p->end_time = get_global_sim_time();

            printf("\033[92m   => FINISH => processPtr=%p => total CPU used=%lu ms => time=%llu ms\033[0m\n",
                   (void*)p,
                   (unsigned long)slice,
                   (unsigned long long)os_time());
            usleep(300000);
        }
    }
    return NULL;
}

/* We'll store process array globally for stats finalize. */
static process_t* g_list=NULL;
static int        g_count=0;

static void finalize(void){
    if(g_count<=0 || HPC_over_mode){
        return;
    }
    /* compute wait, TAT, response for each process. */
    for(int i=0; i<g_count; i++){
        process_t* P = &g_list[i];
        uint64_t at  = P->arrival_time;
        uint64_t st  = P->first_response;
        uint64_t et  = P->end_time;
        uint64_t bt  = P->burst_time; /* for reference */

        /* Wait = (start - arrival). */
        uint64_t wait = (st > at) ? (st - at) : 0ULL;
        /* TAT = end - arrival. */
        uint64_t tat  = (et > at) ? (et - at) : 0ULL;
        /* Resp = (first_response - arrival). */
        uint64_t resp = wait; /* same as wait here. */

        gStats.total_wait       += wait;
        gStats.total_turnaround += tat;
        gStats.total_response   += resp;
    }
    unsigned long long n = gStats.total_processes;
    if(n>0){
        gAvgWait = (double)gStats.total_wait/(double)n;
        gAvgTAT  = (double)gStats.total_turnaround/(double)n;
        gAvgResp = (double)gStats.total_response/(double)n;
    }
    gPreemptions = gStats.preemptions;
    gProcs       = n;
}

void scheduler_select_algorithm(scheduler_alg_t a){
    current_alg=a;
}

void scheduler_run(process_t* list,int count){
    reset_accumulators();
    if(!list || count<=0){
        return;
    }

    /* Special HPC overshadow mode => no normal stats. */
    if(current_alg==ALG_HPC_OVERSHADOW){
        HPC_over_mode=1;
        printf("\n\033[95m$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
        printf("SCHEDULE NAME => HPC-OVERSHADOW\n");
        printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\033[0m\n");
        usleep(300000);

        os_run_hpc_overshadow();

        printf("\033[96m$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
        printf("SCHEDULE END => HPC-OVERSHADOW => no normal stats.\n");
        printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\033[0m\n");
        usleep(300000);
        return;
    }

    /* else normal scheduling. Print big block start. */
    printf("\n\033[95m$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    printf("SCHEDULE NAME => %d (enum)\n", current_alg);
    printf("Number of processes=%d\n", count);
    printf("Time start=%llu ms\n", (unsigned long long)os_time());
    printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\033[0m\n");
    usleep(300000);

    ready_queue_init_policy(current_alg);
    g_list = list;
    g_count= count;

    /* We default to 1 core in most tests. If you want multi-core in tests, you can set it. */
    g_num_cores = 1;
    if(current_alg==ALG_BFS){
        /* BFS is interesting with multiple cores. Let's do 2 just for demonstration. */
        g_num_cores = 2;
    }

    g_running=1;
    /* Initialize each process arrival => push to queue. */
    for(int i=0;i<count;i++){
        ready_queue_push(&list[i]);
        gStats.total_processes++;
    }

    /* Start core threads. */
    pthread_t tid[MAX_CORES];
    int n = (g_num_cores>MAX_CORES?MAX_CORES:g_num_cores);
    for(int i=0;i<n;i++){
        pthread_create(&tid[i],NULL,scheduling_core_thread,(void*)(long)i);
    }

    /* Wait until queue empties => we watch size periodically. */
    while( ready_queue_size()>0 ){
        usleep(200000);
    }
    /* Then stop the scheduling threads. */
    g_running=0;
    /* So they unblock from pop => we do cond_signal multiple times. */
    for(int k=0;k<n;k++){
        /* push a dummy? or just signal broadcast. */
        ready_queue_push(NULL);
    }
    for(int i=0;i<n;i++){
        pthread_join(tid[i],NULL);
    }

    ready_queue_destroy();
    finalize();

    /* End ASCII block. */
    uint64_t total_time = get_global_sim_time();
    printf("\033[96m$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    printf("SCHEDULE END => alg=%d => totalTime=%llums\n", current_alg,
           (unsigned long long)total_time);
    printf("Stats: preemptions=%llu, totalProcs=%llu\n",
           (unsigned long long)gPreemptions,
           (unsigned long long)gProcs);
    printf("AvgWait=%.2f, AvgTAT=%.2f, AvgResp=%.2f\n",
           gAvgWait, gAvgTAT, gAvgResp);
    printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\033[0m\n");
    usleep(300000);

    /* Cleanup references. */
    g_list=NULL;
    g_count=0;
}

void scheduler_fetch_report(sched_report_t* out){
    if(!out) return;
    if(HPC_over_mode){
        out->avg_wait=0.0;
        out->avg_turnaround=0.0;
        out->avg_response=0.0;
        out->preemptions=0ULL;
        out->total_procs=0ULL;
    } else {
        out->avg_wait       = gAvgWait;
        out->avg_turnaround = gAvgTAT;
        out->avg_response   = gAvgResp;
        out->preemptions    = gPreemptions;
        out->total_procs    = gProcs;
    }
}
