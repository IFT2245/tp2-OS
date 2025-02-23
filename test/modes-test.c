#include "modes-test.h"
#include "test_common.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include <string.h>
#include <stdio.h>

static int tests_run=0, tests_failed=0;

static void hpc_over(void){
    os_init();
    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    process_t d[1];
    init_process(&d[0], 0, 0, os_time());
    scheduler_run(d,1);
    os_cleanup();
}
TEST(test_hpc_over){
    struct captured_output cap;
    int st = run_function_capture_output(hpc_over, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "HPC overshadow start")
                       && strstr(cap.stdout_buf, "HPC overshadow done"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "HPC overshadow logs or concurrency timeline missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HPC_OVERSHADOW);
    return true;
}

static void multi_containers(void){
    os_init();
    for(int i=0;i<2;i++){
        os_create_ephemeral_container();
    }
    for(int i=0;i<2;i++){
        os_remove_ephemeral_container();
    }
    os_cleanup();
}
TEST(test_multi_containers){
    struct captured_output cap;
    int st = run_function_capture_output(multi_containers, &cap);
    bool pass = (st==0 && strstr(cap.stdout_buf, "Container created")
                       && strstr(cap.stdout_buf, "Container removed"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "Containers logs missing or incomplete.");
        return false;
    }
    return true;
}

static void multi_distrib(void){
    os_init();
    for(int i=0;i<2;i++){
        os_run_distributed_example();
    }
    os_cleanup();
}
TEST(test_multi_distrib){
    struct captured_output cap;
    int st=run_function_capture_output(multi_distrib, &cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Distributed example: fork"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "Multi distributed logs missing.");
        return false;
    }
    return true;
}

static void pipeline_modes(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
}
TEST(test_pipeline_modes){
    struct captured_output cap;
    int st=run_function_capture_output(pipeline_modes,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Pipeline start")
                     && strstr(cap.stdout_buf,"Pipeline end"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "Pipeline logs missing or incomplete in modes test.");
        return false;
    }
    return true;
}

static void mix_algos(void){
    os_init();
    process_t p[2];
    init_process(&p[0], 2, 1, os_time());
    init_process(&p[1], 3, 1, os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,2);
    os_cleanup();
}
TEST(test_mix_algos){
    struct captured_output cap;
    int st=run_function_capture_output(mix_algos, &cap);
    bool pass=(st==0
               && strstr(cap.stdout_buf,"Stats for FIFO")
               && strstr(cap.stdout_buf,"Stats for BFS"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "mix_algos logs/timeline missing for FIFO/BFS.");
        return false;
    }
    return true;
}

static void double_hpc(void){
    os_init();
    os_run_hpc_overshadow();
    os_run_hpc_overshadow();
    os_cleanup();
}
TEST(test_double_hpc){
    struct captured_output cap;
    int st=run_function_capture_output(double_hpc,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"HPC overshadow start"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "Double HPC overshadow logs missing or incomplete.");
        return false;
    }
    return true;
}

static void mlfq_check(void){
    os_init();
    process_t p[3];
    for(int i=0; i<3; i++){
        init_process(&p[i], 3+i, (i+1)*10, os_time());
    }
    scheduler_select_algorithm(ALG_MLFQ);
    scheduler_run(p,3);
    os_cleanup();
}
TEST(test_mlfq_check){
    struct captured_output cap;
    int st=run_function_capture_output(mlfq_check, &cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for MLFQ"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "MLFQ logs/timeline missing or incomplete.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_MLFQ);
    return true;
}

void run_modes_tests(int* total,int* passed){
    tests_run=0;
    tests_failed=0;

    RUN_TEST(test_hpc_over);
    RUN_TEST(test_multi_containers);
    RUN_TEST(test_multi_distrib);
    RUN_TEST(test_pipeline_modes);
    RUN_TEST(test_mix_algos);
    RUN_TEST(test_double_hpc);
    RUN_TEST(test_mlfq_check);

    *total = tests_run;
    *passed = tests_run - tests_failed;
}
