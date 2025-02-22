#include "basic-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include <string.h>


static int tests_run=0, tests_failed=0;

static void sc_fifo_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,os_time());
    init_process(&p[1],5,1,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    os_cleanup();
}
TEST(test_fifo){
    struct captured_output cap;
    int st=run_function_capture_output(sc_fifo_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Init") && strstr(cap.stdout_buf,"Stats for FIFO") && strstr(cap.stdout_buf,"Cleanup"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"FIFO logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_FIFO);
    return true;
}

static void sc_rr_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],2,1,os_time());
    init_process(&p[1],2,1,os_time());
    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,2);
    os_cleanup();
}
TEST(test_rr){
    struct captured_output cap;
    int st=run_function_capture_output(sc_rr_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Init") && strstr(cap.stdout_buf,"Stats for RR") && strstr(cap.stdout_buf,"Cleanup"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"RR logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_RR);
    return true;
}

static void sc_cfs_run(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,0,os_time());
    init_process(&p[1],4,0,os_time());
    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(p,2);
    os_cleanup();
}
TEST(test_cfs){
    struct captured_output cap;
    int st=run_function_capture_output(sc_cfs_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for CFS"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"CFS logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS);
    return true;
}

static void sc_bfs_run(void){
    os_init();
    process_t p[3];
    for(int i=0;i<3;i++) init_process(&p[i],2+i,1,os_time());
    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,3);
    os_cleanup();
}
TEST(test_bfs){
    struct captured_output cap;
    int st=run_function_capture_output(sc_bfs_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for BFS"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"BFS logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_BFS);
    return true;
}

static void sc_pipeline_run(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
}
TEST(test_pipeline){
    struct captured_output cap;
    int st=run_function_capture_output(sc_pipeline_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Pipeline start") && strstr(cap.stdout_buf,"Pipeline end"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Pipeline logs missing.");
        return false;
    }
    return true;
}

static void sc_distributed_run(void){
    os_init();
    os_run_distributed_example();
    os_cleanup();
}
TEST(test_distributed){
    struct captured_output cap;
    int st=run_function_capture_output(sc_distributed_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Distributed example: fork"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"Distributed logs missing.");
        return false;
    }
    return true;
}

static void sc_fifo_strict(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,10,os_time());
    init_process(&p[1],4,20,os_time());
    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);
    os_cleanup();
}
TEST(test_fifo_strict){
    struct captured_output cap;
    int st=run_function_capture_output(sc_fifo_strict,&cap);
    if(st!=0){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"FIFO strict aborted.");
        return false;
    }
    bool p10=false,p20=false;
    bool violate=false;
    char* line=strtok(cap.stdout_buf,"\n");
    while(line){
        if(strstr(line,"[Worker]")&&strstr(line,"priority=")){
            int prio=atoi(strstr(line,"priority=")+9);
            if(prio==20) p20=true;
            if(prio==10 && p20) violate=true;
        }
        line=strtok(NULL,"\n");
    }
    if(violate){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"FIFO strict violation order.");
        return false;
    }
    return true;
}

void run_basic_tests(int* total,int* passed){
    tests_run=0; tests_failed=0;
    RUN_TEST(test_fifo);
    RUN_TEST(test_rr);
    RUN_TEST(test_cfs);
    RUN_TEST(test_bfs);
    RUN_TEST(test_pipeline);
    RUN_TEST(test_distributed);
    RUN_TEST(test_fifo_strict);
    *total=tests_run; *passed=tests_run-tests_failed;
}
