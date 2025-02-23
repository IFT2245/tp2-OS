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
    unsigned long long preemptions;
    unsigned long long total_processes;
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
    int      started;
} track_t;
static track_t* gTrack=NULL;

typedef struct {
    uint64_t start_ms;
    uint64_t end_ms;
    int      p_index;
} exec_event_t;
static exec_event_t* events=NULL;
static int event_cap=0, event_count=0;

/* Ensure we have space in events[] array. */
static void ensure_event_cap(void){
    if(event_count>=event_cap){
        int new_cap=(event_cap==0)?64:event_cap*2;
        events=(exec_event_t*)realloc(events,new_cap*sizeof(exec_event_t));
        memset(events+event_cap,0,(new_cap-event_cap)*sizeof(exec_event_t));
        event_cap=new_cap;
    }
}

static void add_event(int p_idx, uint64_t start, uint64_t end){
    ensure_event_cap();
    events[event_count].start_ms=start;
    events[event_count].end_ms=end;
    events[event_count].p_index=p_idx;
    event_count++;
}

static void rec_arrival(int i){
    if(gTrack && gTrack[i].arrival==0){
        gTrack[i].arrival=os_time();
    }
}
static void rec_start(int i){
    if(gTrack && !gTrack[i].started){
        gTrack[i].start=os_time();
        gTrack[i].started=1;
    }
}
static void rec_cpu(int i,unsigned long s){
    if(gTrack) gTrack[i].cpu += s;
}
static void rec_end(int i){
    if(gTrack && gTrack[i].end==0){
        gTrack[i].end=os_time();
    }
}

/* Convert alg enum to string. */
static const char* alg2str(scheduler_alg_t x){
    switch(x){
    case ALG_FIFO:         return "FIFO";
    case ALG_RR:           return "RR";
    case ALG_CFS:          return "CFS";
    case ALG_CFS_SRTF:     return "CFS-SRTF";
    case ALG_BFS:          return "BFS";
    case ALG_SJF:          return "SJF";
    case ALG_STRF:         return "STRF";
    case ALG_HRRN:         return "HRRN";
    case ALG_HRRN_RT:      return "HRRN-RT";
    case ALG_PRIORITY:     return "PRIORITY";
    case ALG_HPC_OVERSHADOW: return "HPC-OVER";
    case ALG_MLFQ:         return "MLFQ";
    default:               return "???";
    }
}

static void finalize(void){
    for(int i=0;i<gCount;i++){
        uint64_t at=gTrack[i].arrival;
        uint64_t st=gTrack[i].start;
        uint64_t et=gTrack[i].end;
        uint64_t cpu=gTrack[i].cpu;
        uint64_t turn=(et>at?(et-at):0);
        uint64_t resp=(st>at?(st-at):0);
        uint64_t wait=(turn>cpu?(turn-cpu):0);
        gStats.total_turnaround+=turn;
        gStats.total_response+=resp;
        gStats.total_wait+=wait;
    }
}

void scheduler_select_algorithm(scheduler_alg_t a){
    current_alg=a;
}

/* Put all processes in the ready queue, record arrivals. */
static void enqueue_all(process_t* arr,int n){
    for(int i=0;i<n;i++){
        ready_queue_push(&arr[i]);
        gStats.total_processes++;
        rec_arrival(i);
    }
}

/* Compare function for sorting local concurrency events by start time. */
static int cmp_events(const void* a,const void* b){
    exec_event_t* xa=(exec_event_t*)a;
    exec_event_t* xb=(exec_event_t*)b;
    if(xa->start_ms<xb->start_ms) return -1;
    if(xa->start_ms>xb->start_ms) return 1;
    return 0;
}

/*
  scheduler_run() => local concurrency timeline from events[]:
  1) If HPC-OVER, just run overshadow and skip queue logic
  2) Otherwise, init queue with given algorithm
  3) Repeatedly pop => simulate => push or finish => record concurrency events
  4) Print final timeline
*/
void scheduler_run(process_t* list,int count){
    if(!list||count<=0) return;
    gCount=count;
    gTrack=(track_t*)calloc(count,sizeof(track_t));
    memset(&gStats,0,sizeof(gStats));
    free(events); events=NULL; event_cap=0; event_count=0;

    if(current_alg==ALG_HPC_OVERSHADOW){
        os_run_hpc_overshadow();
        free(gTrack); gTrack=NULL;
        return;
    }

    ready_queue_init_policy(current_alg);
    enqueue_all(list,count);

    uint64_t t0=os_time();
    int quantum=2;
    while(ready_queue_size()>0){
        process_t* p=ready_queue_pop();
        if(!p) break;
        int idx=(int)(p-list);
        rec_start(idx);
        uint64_t st_block=os_time();

        if(current_alg==ALG_MLFQ||current_alg==ALG_BFS||current_alg==ALG_RR||
           current_alg==ALG_CFS_SRTF||current_alg==ALG_STRF||current_alg==ALG_HRRN_RT){
            if(p->remaining_time>(unsigned long)quantum){
                simulate_process_partial(p,quantum);
                rec_cpu(idx,quantum);
                p->remaining_time-=quantum;
                if(current_alg==ALG_CFS_SRTF) p->vruntime+=quantum;
                gStats.preemptions++;
                if(current_alg==ALG_MLFQ) p->mlfq_level++;
                uint64_t en_block=os_time();
                add_event(idx, st_block, en_block);
                ready_queue_push(p);
            } else {
                simulate_process_partial(p,p->remaining_time);
                rec_cpu(idx,p->remaining_time);
                uint64_t en_block=os_time();
                add_event(idx, st_block, en_block);
                p->remaining_time=0;
                rec_end(idx);
            }
        } else {
            /* FIFO, PRIORITY, BFS(non-preempt?), etc. => just run to completion. */
            simulate_process(p);
            rec_cpu(idx,p->remaining_time);
            uint64_t en_block=os_time();
            add_event(idx, st_block, en_block);
            p->remaining_time=0;
            rec_end(idx);
        }
    }
    ready_queue_destroy();

    uint64_t tot=os_time()-t0;
    finalize();
    printf("\033[92mStats for %s => time=%llu\n\033[0m",
           alg2str(current_alg),(unsigned long long)tot);
    printf("procs=%llu preempt=%llu\n",gStats.total_processes,gStats.preemptions);
    if(gStats.total_processes>0){
        double n=(double)gStats.total_processes;
        double avgW=gStats.total_wait/n;
        double avgT=gStats.total_turnaround/n;
        double avgR=gStats.total_response/n;
        printf("Wait=%.2f TAT=%.2f RESP=%.2f\n",avgW,avgT,avgR);
    }

    /* local concurrency timeline from events[] => show partial runs. */
    if(event_count>0){
        printf("\n\033[93m--- Local Concurrency Timeline [%s] ---\033[0m\n",
               alg2str(current_alg));
        qsort(events,event_count,sizeof(exec_event_t),cmp_events);

        /* find min_s, max_e. */
        uint64_t min_s=(uint64_t)-1, max_e=0ULL;
        for(int i=0;i<event_count;i++){
            if(events[i].start_ms<min_s) min_s=events[i].start_ms;
            if(events[i].end_ms>max_e) max_e=events[i].end_ms;
        }
        if(min_s==(uint64_t)-1) min_s=0;
        if(max_e<min_s) max_e=min_s;

        const int COLS=30;
        uint64_t span=(max_e>min_s?(max_e-min_s):1ULL);

        /* print each event => partial or full run. */
        for(int i=0;i<event_count;i++){
            char row[COLS+1];
            memset(row,' ',sizeof(row));
            row[COLS]='\0';
            uint64_t s=events[i].start_ms;
            uint64_t e=events[i].end_ms;
            for(int c=0;c<COLS;c++){
                uint64_t t=min_s + (c*span/COLS);
                if(t>=s && t<e){
                    row[c]='█';
                }
            }
            uint64_t dur=(e>s?(e-s):0ULL);
            printf("P%d:[%s] start=%llu end=%llu dur=%llums\n",
                   events[i].p_index+1,row,
                   (unsigned long long)s,
                   (unsigned long long)e,
                   (unsigned long long)dur);
        }
        printf("\n");
    }

    free(events);  events=NULL;  event_cap=0;  event_count=0;
    free(gTrack);  gTrack=NULL;  gCount=0;
}
