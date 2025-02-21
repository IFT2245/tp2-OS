#include "modes-test.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/process.h"
#include <stdio.h>

static int tot=0,ok=0;

static void t_hpc_over(void){
    tot++;
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    process_t dummy[1];
    init_process(&dummy[0],0,0,os_time());
    scheduler_run(dummy,1);
    ok++;
}

static void t_multi_container(void){
    tot++;
    os_create_ephemeral_container();
    os_create_ephemeral_container();
    os_remove_ephemeral_container();
    os_remove_ephemeral_container();
    ok++;
}

static void t_multi_distributed(void){
    tot++;
    for(int i=0;i<2;i++){
        os_run_distributed_example();
    }
    ok++;
}

static void t_pipeline_modes(void){
    tot++;
    os_pipeline_example();
    ok++;
}

static void t_mix_algos(void){
    tot++;
    process_t p[2];
    init_process(&p[0],2,1,os_time());
    init_process(&p[1],3,1,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,2);
    ok++;
}

static void t_double_hpc(void){
    tot++;
    os_run_hpc_overshadow();
    os_run_hpc_overshadow();
    ok++;
}

void run_modes_tests(int* total,int* passed){
    t_hpc_over();
    t_multi_container();
    t_multi_distributed();
    t_pipeline_modes();
    t_mix_algos();
    t_double_hpc();
    *total=tot;*passed=ok;
}
