#include "normal-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include <string.h>

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
TEST(test_sjf){
    struct captured_output cap; int st=run_function_capture_output(sc_sjf_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for SJF"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"SJF logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_SJF);
    return true;
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
TEST(test_strf){
    struct captured_output cap; int st=run_function_capture_output(sc_strf_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for STRF"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"STRF logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_STRF);
    return true;
}

static void sc_hrrn_run(void){
    os_init();
    process_t p[3];
    for(int i=0;i<3;i++){ init_process(&p[i],2+i,1,os_time()); }
    scheduler_select_algorithm(ALG_HRRN);
    scheduler_run(p,3);
    os_cleanup();
}
TEST(test_hrrn){
    struct captured_output cap; int st=run_function_capture_output(sc_hrrn_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for HRRN"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"HRRN logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN);
    return true;
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
TEST(test_hrrn_rt){
    struct captured_output cap; int st=run_function_capture_output(sc_hrrn_rt_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for HRRN-RT"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"HRRN-RT logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN_RT);
    return true;
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
TEST(test_prio){
    struct captured_output cap; int st=run_function_capture_output(sc_prio_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for PRIORITY"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"PRIORITY logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_PRIORITY);
    return true;
}

static void sc_cfs_srtf_run(void){
    os_init();
    process_t p[3];
    for(int i=0;i<3;i++){ init_process(&p[i],2+(i*2),1,os_time()); }
    scheduler_select_algorithm(ALG_CFS_SRTF);
    scheduler_run(p,3);
    os_cleanup();
}
TEST(test_cfs_srtf){
    struct captured_output cap; int st=run_function_capture_output(sc_cfs_srtf_run,&cap);
    bool pass=(st==0 && strstr(cap.stdout_buf,"Stats for CFS-SRTF"));
    if(!pass){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"CFS-SRTF logs missing.");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS_SRTF);
    return true;
}

static void sc_sjf_strict(void){
    os_init();
    process_t p[2];
    init_process(&p[0],2,10,os_time());
    init_process(&p[1],5,20,os_time());
    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);
    os_cleanup();
}
TEST(test_sjf_strict){
    struct captured_output cap; int st=run_function_capture_output(sc_sjf_strict,&cap);
    if(st!=0){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"SJF strict fail run.");
        return false;
    }
    bool p10done=false, p20=false;
    char* line=strtok(cap.stdout_buf,"\n");
    while(line){
        if(strstr(line,"[Worker] Partial")||strstr(line,"[Worker] Full")){
            int prio=0; char* c=strstr(line,"priority=");
            if(c) prio=atoi(c+9);
            if(prio==20 && !p10done) p20=true;
            if(prio==10 && p20){
                snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),"SJF strict order fail (p10 after p20).");
                return false;
            }
            if(prio==10 && strstr(line,"burst=2")) p10done=true;
        }
        line=strtok(NULL,"\n");
    }
    return true;
}

void run_normal_tests(int* total,int* passed){
    tests_run=0; tests_failed=0;
    RUN_TEST(test_sjf);
    RUN_TEST(test_strf);
    RUN_TEST(test_hrrn);
    RUN_TEST(test_hrrn_rt);
    RUN_TEST(test_prio);
    RUN_TEST(test_cfs_srtf);
    RUN_TEST(test_sjf_strict);
    *total=tests_run; *passed=tests_run-tests_failed;
}
