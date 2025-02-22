#include "edge-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include <string.h>

static int tests_run=0, tests_failed=0;

static void extreme_long(void){
    os_init();
    process_t p[1]; init_process(&p[0],50,2,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,1);
    os_cleanup();
}
TEST(test_extreme_long){
    struct captured_output cap; int st=run_function_capture_output(extreme_long,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for FIFO"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Long burst logs missing.");
        return false;
    }
    return true;
}

static void extreme_short(void){
    os_init();
    process_t p[1]; init_process(&p[0],1,2,os_time());
    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,1);
    os_cleanup();
}
TEST(test_extreme_short){
    struct captured_output cap; int st=run_function_capture_output(extreme_short,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for RR"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Short burst logs missing.");
        return false;
    }
    return true;
}

static void high_load(void){
    os_init();
    process_t arr[10]; for(int i=0;i<10;i++){ init_process(&arr[i],3+(i%3),1,os_time()); }
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(arr,10);
    os_cleanup();
}
TEST(test_high_load){
    struct captured_output cap; int st=run_function_capture_output(high_load,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for CFS"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"High load logs missing.");
        return false;
    }
    return true;
}

static void hpc_under_load(void){
    os_init(); os_run_hpc_overshadow(); os_cleanup();
}
TEST(test_hpc_under_load){
    struct captured_output cap; int st=run_function_capture_output(hpc_under_load,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"HPC overshadow start") && strstr(cap.stdout_buf,"HPC overshadow done"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"HPC overshadow logs missing.");
        return false;
    }
    return true;
}

static void container_spam(void){
    os_init();
    for(int i=0;i<3;i++){ os_create_ephemeral_container(); }
    for(int i=0;i<3;i++){ os_remove_ephemeral_container(); }
    os_cleanup();
}
TEST(test_container_spam){
    struct captured_output cap; int st=run_function_capture_output(container_spam,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Container created") && strstr(cap.stdout_buf,"Container removed"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Container spam logs missing.");
        return false;
    }
    return true;
}

static void pipeline_edge(void){
    os_init(); os_pipeline_example(); os_cleanup();
}
TEST(test_pipeline_edge){
    struct captured_output cap; int st=run_function_capture_output(pipeline_edge,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Pipeline start") && strstr(cap.stdout_buf,"Pipeline end"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Pipeline edge logs missing.");
        return false;
    }
    return true;
}

static void multi_distrib(void){
    os_init();
    for(int i=0;i<3;i++){ os_run_distributed_example(); }
    os_cleanup();
}
TEST(test_multi_distrib){
    struct captured_output cap; int st=run_function_capture_output(multi_distrib,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Distributed example: fork"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Multi distributed logs missing.");
        return false;
    }
    return true;
}

void run_edge_tests(int* total,int* passed){
    tests_run=0; tests_failed=0;
    RUN_TEST(test_extreme_long);
    RUN_TEST(test_extreme_short);
    RUN_TEST(test_high_load);
    RUN_TEST(test_hpc_under_load);
    RUN_TEST(test_container_spam);
    RUN_TEST(test_pipeline_edge);
    RUN_TEST(test_multi_distrib);
    *total=tests_run; *passed=tests_run-tests_failed;
}
