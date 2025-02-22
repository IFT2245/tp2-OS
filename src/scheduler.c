#include "scheduler.h"
#include "ready_queue.h"
#include "os.h"
#include "worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static scheduler_alg_t current_alg=ALG_CFS;
static int gCount=0;

typedef struct {
    unsigned long preemptions;
    unsigned long total_processes;
    unsigned long long total_wait;
    unsigned long long total_turnaround;
    unsigned long long total_response;
} sched_stats_t;
static sched_stats_t gStats;

typedef struct {
    uint64_t arrival;
    uint64_t start;
    uint64_t end;
    uint64_t cpu;
    int started;
} track_t;
static track_t* gTrack=NULL;

static void rec_arrival(int i){ if(gTrack && gTrack[i].arrival==0) gTrack[i].arrival=os_time(); }
static void rec_start(int i){ if(gTrack && !gTrack[i].started){ gTrack[i].start=os_time(); gTrack[i].started=1;} }
static void rec_cpu(int i,unsigned long s){ if(gTrack) gTrack[i].cpu+=s; }
static void rec_end(int i){ if(gTrack && gTrack[i].end==0) gTrack[i].end=os_time(); }
static void finalize(void){
    for(int i=0;i<gCount;i++){
        uint64_t at=gTrack[i].arrival;
        uint64_t st=gTrack[i].start;
        uint64_t et=gTrack[i].end;
        uint64_t cpu=gTrack[i].cpu;
        uint64_t turn=(et>at)?(et-at):0;
        uint64_t resp=(st>at)?(st-at):0;
        uint64_t wait=(turn>cpu)?(turn-cpu):0;
        gStats.total_turnaround+=turn;
        gStats.total_response+=resp;
        gStats.total_wait+=wait;
    }
}

static const char* alg2str(scheduler_alg_t x){
    switch(x){
    case ALG_FIFO: return "FIFO";
    case ALG_RR: return "RR";
    case ALG_CFS: return "CFS";
    case ALG_CFS_SRTF: return "CFS-SRTF";
    case ALG_BFS: return "BFS";
    case ALG_SJF: return "SJF";
    case ALG_STRF: return "STRF";
    case ALG_HRRN: return "HRRN";
    case ALG_HRRN_RT: return "HRRN-RT";
    case ALG_PRIORITY: return "PRIORITY";
    case ALG_HPC_OVERSHADOW: return "HPC-OVER";
    case ALG_MLFQ: return "MLFQ";
    default: return "???";
    }
}

void scheduler_select_algorithm(scheduler_alg_t a){ current_alg=a; }

static void enqueue_all(process_t* arr,int n){
    for(int i=0;i<n;i++){
        ready_queue_push(&arr[i]);
        gStats.total_processes++;
        rec_arrival(i);
    }
}

void scheduler_run(process_t* list,int count){
    if(!list||count<=0)return;
    gCount=count; gTrack=(track_t*)calloc(count,sizeof(track_t));
    memset(&gStats,0,sizeof(gStats));
    if(current_alg==ALG_HPC_OVERSHADOW){
        os_run_hpc_overshadow();
        free(gTrack); gTrack=NULL; return;
    }
    ready_queue_init_policy(current_alg);
    enqueue_all(list,count);
    uint64_t t0=os_time();

    int quantum=2;
    while(ready_queue_size()>0){
        process_t* p=ready_queue_pop();
        if(!p)break;
        int idx=(int)(p-list);
        rec_start(idx);
        if(current_alg==ALG_MLFQ||current_alg==ALG_BFS||current_alg==ALG_RR||
           current_alg==ALG_CFS_SRTF||current_alg==ALG_STRF||current_alg==ALG_HRRN_RT){
            if(p->remaining_time>quantum){
                simulate_process_partial(p,quantum);
                rec_cpu(idx,quantum);
                p->remaining_time-=quantum;
                if(current_alg==ALG_CFS_SRTF) p->vruntime+=quantum;
                gStats.preemptions++;
                if(current_alg==ALG_MLFQ) p->mlfq_level++;
                ready_queue_push(p);
            } else {
                simulate_process_partial(p,p->remaining_time);
                rec_cpu(idx,p->remaining_time);
                p->remaining_time=0; rec_end(idx);
            }
        } else {
            simulate_process(p);
            rec_cpu(idx,p->remaining_time);
            p->remaining_time=0; rec_end(idx);
        }
    }
    ready_queue_destroy();
    uint64_t tot=os_time()-t0;
    finalize();
    printf("\033[92mStats for %s => time=%llu\n\033[0m", alg2str(current_alg),(unsigned long long)tot);
    printf("procs=%lu preempt=%lu\n", gStats.total_processes,gStats.preemptions);
    if(gStats.total_processes>0){
        double n=(double)gStats.total_processes;
        double aW=gStats.total_wait/n; double aT=gStats.total_turnaround/n; double aR=gStats.total_response/n;
        printf("Wait=%.2f TAT=%.2f RESP=%.2f\n",aW,aT,aR);
    }
    free(gTrack); gTrack=NULL; gCount=0;
}
