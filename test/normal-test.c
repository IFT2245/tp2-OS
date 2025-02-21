#include "normal-test.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include <stdio.hx>

static int tot=0,ok=0;

static void t_sjf_simple(void){
    tot++;
    process_t p[3];
    init_process(&p[0],1,1,os_time());
    init_process(&p[1],5,1,os_time());
    init_process(&p[2],2,1,os_time());
    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,3);
    ok++;
}

static void t_strf_simple(void){
    tot++;
    process_t p[2];
    init_process(&p[0],4,1,os_time());
    init_process(&p[1],3,1,os_time());
    scheduler_select_algorithm(ALG_STRF);
    scheduler_run(p,2);
    ok++;
}

static void t_hrrn_simple(void){
    tot++;
    process_t p[3];
    for(int i=0;i<3;i++){
        init_process(&p[i],2+i,1,os_time());
    }
    scheduler_select_algorithm(ALG_HRRN);
    scheduler_run(p,3);
    ok++;
}

static void t_hrrn_rt_simple(void){
    tot++;
    process_t p[3];
    for(int i=0;i<3;i++){
        init_process(&p[i],2,(i==2?2:1),os_time());
    }
    scheduler_select_algorithm(ALG_HRRN_RT);
    scheduler_run(p,3);
    ok++;
}

static void t_priority_mode(void){
    tot++;
    process_t p[3];
    init_process(&p[0],2,3,os_time());
    init_process(&p[1],2,1,os_time());
    init_process(&p[2],2,2,os_time());
    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,3);
    ok++;
}

static void t_cfs_srtf_mode(void){
    tot++;
    process_t p[3];
    init_process(&p[0],3,1,os_time());
    init_process(&p[1],4,1,os_time());
    init_process(&p[2],1,1,os_time());
    scheduler_select_algorithm(ALG_CFS_SRTF);
    scheduler_run(p,3);
    ok++;
}

void run_normal_tests(int* total,int* passed){
    t_sjf_simple();
    t_strf_simple();
    t_hrrn_simple();
    t_hrrn_rt_simple();
    t_priority_mode();
    t_cfs_srtf_mode();
    *total=tot;*passed=ok;
}
