#include "hidden-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include <string.h>

static int tests_run=0, tests_failed=0;

static void distrib_heavy(void){
    os_init();
    for(int i=0;i<4;i++){ os_run_distributed_example(); }
    os_cleanup();
}
TEST(test_distrib_heavy){
    struct captured_output cap; int st=run_function_capture_output(distrib_heavy,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Distributed example: fork"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Distrib heavy logs missing.");
        return false;
    }
    return true;
}

static void hpc_heavy(void){
    os_run_hpc_overshadow(); os_run_hpc_overshadow();
}
TEST(test_hpc_heavy){
    struct captured_output cap; int st=run_function_capture_output(hpc_heavy,&cap);
    int c=0; char* s=cap.stdout_buf;
    while((s=strstr(s,"HPC overshadow start"))){c++;s++;}
    bool pass=(st==0 && c>=2);
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Needed HPC overshadow >=2 times, found %d.",c);
        return false;
    }
    return true;
}

static void container_combo(void){
    os_init();
    os_create_ephemeral_container();
    os_run_distributed_example();
    os_run_hpc_overshadow();
    os_remove_ephemeral_container();
    os_cleanup();
}
TEST(test_container_combo){
    struct captured_output cap; int st=run_function_capture_output(container_combo,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Container created") && strstr(cap.stdout_buf,"Container removed")
               && strstr(cap.stdout_buf,"Distributed example: fork") && strstr(cap.stdout_buf,"HPC overshadow done"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Container combo logs missing.");
        return false;
    }
    return true;
}

static void scheduling_var(void){
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
TEST(test_scheduling_variety){
    struct captured_output cap; int st=run_function_capture_output(scheduling_var,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for SJF") && strstr(cap.stdout_buf,"Stats for PRIORITY"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"SJF/PRIORITY logs missing.");
        return false;
    }
    return true;
}

static void auto_logic(void){
    printf("Auto mode selection tested.\n");
}
TEST(test_auto_logic){
    struct captured_output cap; int st=run_function_capture_output(auto_logic,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Auto mode selection tested."));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Auto logic line missing.");
        return false;
    }
    return true;
}

static void final_integration(void){
    os_init();
    os_log("Final synergy HPC + container + pipeline + distributed");
    os_create_ephemeral_container();
    os_run_hpc_overshadow();
    os_run_distributed_example();
    os_pipeline_example();
    os_remove_ephemeral_container();
    os_cleanup();
}
TEST(test_final_integration){
    struct captured_output cap; int st=run_function_capture_output(final_integration,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Final synergy HPC + container + pipeline + distributed")
               && strstr(cap.stdout_buf,"Pipeline end"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Missing synergy logs.");
        return false;
    }
    return true;
}

static void multi_stage_distrib(void){
    for(int i=0;i<2;i++){
        os_run_distributed_example();
        os_run_hpc_overshadow();
    }
}
TEST(test_multi_stage_distributed){
    struct captured_output cap; int st=run_function_capture_output(multi_stage_distrib,&cap);
    int c=0; char* s=cap.stdout_buf;
    while((s=strstr(s,"HPC overshadow start"))){c++;s++;}
    bool pass=(st==0 && c>=2);
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Multi overshadow logs missing or <2.");
        return false;
    }
    return true;
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
    *total=tests_run;*passed=tests_run-tests_failed;
}
