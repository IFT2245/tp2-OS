#include "edge-test.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include <stdio.h>

static int tot=0,ok=0;

static void t_extreme_long_burst(void){
    tot++;
    process_t p[1];
    init_process(&p[0],50,2,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,1);
    ok++;
}

static void t_extreme_short_burst(void){
    tot++;
    process_t p[1];
    init_process(&p[0],1,2,os_time());
    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,1);
    ok++;
}

static void t_high_load(void){
    tot++;
    process_t arr[10];
    for(int i=0;i<10;i++){
        init_process(&arr[i],3+(i%3),1,os_time());
    }
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(arr,10);
    ok++;
}

static void t_hpc_under_load(void){
    tot++;
    os_run_hpc_overshadow();
    ok++;
}

static void t_container_spam(void){
    tot++;
    for(int i=0;i<3;i++){
        os_create_ephemeral_container();
    }
    for(int i=0;i<3;i++){
        os_remove_ephemeral_container();
    }
    ok++;
}

static void t_pipeline_edge(void){
    tot++;
    os_pipeline_example();
    ok++;
}

static void t_extra_distributed_edge(void){
    tot++;
    for(int i=0;i<3;i++){
        os_run_distributed_example();
    }
    ok++;
}

void run_edge_tests(int* total,int* passed){
    t_extreme_long_burst();
    t_extreme_short_burst();
    t_high_load();
    t_hpc_under_load();
    t_container_spam();
    t_pipeline_edge();
    t_extra_distributed_edge();
    *total=tot; *passed=ok;
}
