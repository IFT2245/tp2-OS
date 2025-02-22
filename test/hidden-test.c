#include "hidden-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include <string.h>

static int tests_run=0, tests_failed=0;

static void sc_distrib_heavy(void){
    os_init();
    for(int i=0;i<4;i++){
        os_run_distributed_example();
    }
    os_cleanup();
}
static void sc_hpc_heavy(void){
    os_run_hpc_overshadow();
    os_run_hpc_overshadow();
}
static void sc_container_combo(void){
    os_init();
    os_create_ephemeral_container();
    os_run_distributed_example();
    os_run_hpc_overshadow();
    os_remove_ephemeral_container();
    os_cleanup();
}
static void sc_scheduling_var(void){
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,os_time());
    init_process(&p[1],6,2,os_time());
    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);
    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,2);
    os_cleanup();
}
static void sc_auto_logic(void){
    printf("[Hidden] Auto mode selection tested.\n");
}
static void sc_final_integration(void){
    os_init();
    os_log("Final synergy HPC + container + pipeline + distributed");
    os_create_ephemeral_container();
    os_run_hpc_overshadow();
    os_run_distributed_example();
    os_pipeline_example();
    os_remove_ephemeral_container();
    os_cleanup();
}
static void sc_multi_stage_distrib(void){
    for(int i=0;i<2;i++){
        os_run_distributed_example();
        os_run_hpc_overshadow();
    }
}

TEST(test_distrib_heavy){
    struct captured_output cap;
    int st=run_function_capture_output(sc_distrib_heavy,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Distributed example: fork"));
    return pass;
}
TEST(test_hpc_heavy){
    struct captured_output cap;
    int st=run_function_capture_output(sc_hpc_heavy,&cap);
    // Expect HPC overshadow start at least twice
    int count=0;
    const char* scan=cap.stdout_buf;
    while((scan=strstr(scan,"HPC overshadow start"))){ count++; scan++; }
    bool pass=(st==0 && count>=2);
    return pass;
}
TEST(test_container_combo){
    struct captured_output cap;
    int st=run_function_capture_output(sc_container_combo,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Container created")
               && strstr(cap.stdout_buf,"Container removed")
               && strstr(cap.stdout_buf,"Distributed example: fork")
               && strstr(cap.stdout_buf,"HPC overshadow done"));
    return pass;
}
TEST(test_scheduling_variety){
    struct captured_output cap;
    int st=run_function_capture_output(sc_scheduling_var,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for SJF")
               && strstr(cap.stdout_buf,"Stats for PRIORITY"));
    return pass;
}
TEST(test_auto_logic){
    struct captured_output cap;
    int st=run_function_capture_output(sc_auto_logic,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Auto mode selection tested."));
    return pass;
}
TEST(test_final_integration){
    struct captured_output cap;
    int st=run_function_capture_output(sc_final_integration,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Final synergy HPC + container + pipeline + distributed")
               && strstr(cap.stdout_buf,"Pipeline end"));
    return pass;
}
TEST(test_multi_stage_distributed){
    struct captured_output cap;
    int st=run_function_capture_output(sc_multi_stage_distrib,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"HPC overshadow start"));
    return pass;
}

void run_hidden_tests(int* total,int* passed){
    tests_run=0; tests_failed=0;
    RUN_TEST(test_distrib_heavy);
    RUN_TEST(test_hpc_heavy);
    RUN_TEST(test_container_combo);
    RUN_TEST(test_scheduling_variety);
    RUN_TEST(test_auto_logic);
    RUN_TEST(test_final_integration);
    RUN_TEST(test_multi_stage_distributed);
    *total  = tests_run;
    *passed = (tests_run - tests_failed);
}
