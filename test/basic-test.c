#include "basic-test.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include <stdio.h>

static int tot=0,ok=0;

static void t_fifo_simple(void){
    tot++;
    process_t p[2];
    init_process(&p[0],3,1,os_time());
    init_process(&p[1],5,1,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    ok++;
}

static void t_rr_simple(void){
    tot++;
    process_t p[2];
    init_process(&p[0],2,1,os_time());
    init_process(&p[1],2,1,os_time());
    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,2);
    ok++;
}

static void t_cfs_basic(void){
    tot++;
    process_t p[3];
    for(int i=0;i<3;i++){
        init_process(&p[i],4+i,0,os_time());
    }
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(p,3);
    ok++;
}

static void t_bfs_basic(void){
    tot++;
    process_t p[3];
    for(int i=0;i<3;i++){
        init_process(&p[i],3,0,os_time());
    }
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,3);
    ok++;
}

static void t_pipeline_basic(void){
    tot++;
    os_pipeline_example();
    ok++;
}

static void t_distributed_basic(void){
    tot++;
    os_run_distributed_example();
    ok++;
}

void run_basic_tests(int* total,int* passed){
    t_fifo_simple();
    t_rr_simple();
    t_cfs_basic();
    t_bfs_basic();
    t_pipeline_basic();
    t_distributed_basic();
    *total=tot;*passed=ok;
}
