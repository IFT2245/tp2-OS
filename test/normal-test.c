#include "normal-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/scoreboard.h"

static int tests_run=0, tests_failed=0;

static void sc_sjf_run(void){
    os_init();
    process_t p[3];
    init_process(&p[0],1,1,os_time());
    init_process(&p[1],5,1,os_time());
    init_process(&p[2],2,1,os_time());
    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,3);
    os_cleanup();
}
static void sc_strf_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],4,1,os_time());
    init_process(&p[1],3,1,os_time());
    scheduler_select_algorithm(ALG_STRF);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_hrrn_run(void){
    os_init();
    process_t p[3];
    for(int i=0;i<3;i++){
        init_process(&p[i],2+i,1,os_time());
    }
    scheduler_select_algorithm(ALG_HRRN);
    scheduler_run(p,3);
    os_cleanup();
}
static void sc_hrrn_rt_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,os_time());
    init_process(&p[1],4,2,os_time());
    scheduler_select_algorithm(ALG_HRRN_RT);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_prio_run(void){
    os_init();
    process_t p[3];
    init_process(&p[0],2,3,os_time());
    init_process(&p[1],2,1,os_time());
    init_process(&p[2],2,2,os_time());
    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,3);
    os_cleanup();
}
static void sc_cfs_srtf_run(void){
    os_init();
    process_t p[3];
    for(int i=0;i<3;i++){
        init_process(&p[i],2+(i*2),1,os_time());
    }
    scheduler_select_algorithm(ALG_CFS_SRTF);
    scheduler_run(p,3);
    os_cleanup();
}

TEST(test_sjf){
    struct captured_output cap;
    int st = run_function_capture_output(sc_sjf_run, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for SJF"));
    if(pass) scoreboard_set_sc_mastered(ALG_SJF);
    return pass;
}
TEST(test_strf){
    struct captured_output cap;
    int st = run_function_capture_output(sc_strf_run, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for STRF"));
    if(pass) scoreboard_set_sc_mastered(ALG_STRF);
    return pass;
}
TEST(test_hrrn){
    struct captured_output cap;
    int st = run_function_capture_output(sc_hrrn_run, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for HRRN"));
    if(pass) scoreboard_set_sc_mastered(ALG_HRRN);
    return pass;
}
TEST(test_hrrn_rt){
    struct captured_output cap;
    int st = run_function_capture_output(sc_hrrn_rt_run, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for HRRN-RT"));
    if(pass) scoreboard_set_sc_mastered(ALG_HRRN_RT);
    return pass;
}
TEST(test_prio){
    struct captured_output cap;
    int st = run_function_capture_output(sc_prio_run, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for PRIORITY"));
    if(pass) scoreboard_set_sc_mastered(ALG_PRIORITY);
    return pass;
}
TEST(test_cfs_srtf){
    struct captured_output cap;
    int st = run_function_capture_output(sc_cfs_srtf_run, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Stats for CFS-SRTF"));
    if(pass) scoreboard_set_sc_mastered(ALG_CFS_SRTF);
    return pass;
}

void run_normal_tests(int* total, int* passed){
    tests_run = 0;
    tests_failed = 0;
    RUN_TEST(test_sjf);
    RUN_TEST(test_strf);
    RUN_TEST(test_hrrn);
    RUN_TEST(test_hrrn_rt);
    RUN_TEST(test_prio);
    RUN_TEST(test_cfs_srtf);
    *total  = tests_run;
    *passed = (tests_run - tests_failed);
}
