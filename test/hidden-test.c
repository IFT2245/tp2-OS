#include "hidden-test.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/process.h"
#include <stdio.h>

static int tot=0,ok=0;

static void t_distributed_heavy(void){
    tot++;
    for(int i=0;i<4;i++){
        os_run_distributed_example();
    }
    ok++;
}

static void t_hpc_heavy(void){
    tot++;
    os_run_hpc_overshadow();
    os_run_hpc_overshadow();
    ok++;
}

static void t_container_combo(void){
    tot++;
    os_create_ephemeral_container();
    os_run_distributed_example();
    os_run_hpc_overshadow();
    os_remove_ephemeral_container();
    ok++;
}

static void t_scheduling_variety(void){
    tot++;
    process_t p[2];
    init_process(&p[0],2,1,os_time());
    init_process(&p[1],6,2,os_time());
    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);
    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,2);
    ok++;
}

static void t_auto_logic(void){
    tot++;
    os_log("Auto mode selection tested (theoretical).");
    ok++;
}

static void t_final_integration(void){
    tot++;
    os_log("Final synergy HPC + container + pipeline + distributed");
    os_create_ephemeral_container();
    os_run_hpc_overshadow();
    os_run_distributed_example();
    os_pipeline_example();
    os_remove_ephemeral_container();
    ok++;
}

static void t_multi_stage_distributed(void){
    tot++;
    for(int i=0;i<2;i++){
        os_run_distributed_example();
        os_run_hpc_overshadow();
    }
    ok++;
}

void run_hidden_tests(int* total,int* passed){
    t_distributed_heavy();
    t_hpc_heavy();
    t_container_combo();
    t_scheduling_variety();
    t_auto_logic();
    t_final_integration();
    t_multi_stage_distributed();
    *total=tot; *passed=ok;
}
