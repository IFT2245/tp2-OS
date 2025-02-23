#include "normal-test.h"
#include "test_common.h"
#include "../src/process.h"
#include "../src/scheduler.h"
#include "../src/os.h"
#include "../src/scoreboard.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

static int tests_run=0, tests_failed=0;
extern char g_test_fail_reason[256];

static int almost_equal(double a,double b,double eps){
    return fabs(a-b)<eps;
}
static bool check_stats(const sched_report_t* r,
                        double w, double t, double resp,
                        unsigned long long pre,
                        double eps)
{
    if(!almost_equal(r->avg_wait, w, eps)) return false;
    if(!almost_equal(r->avg_turnaround, t, eps)) return false;
    if(!almost_equal(r->avg_response, resp, eps)) return false;
    if(r->preemptions != pre) return false;
    return true;
}

/* SJF => 3 procs => p0=1, p1=5, p2=2 =>
   order => p0->p2->p1 =>
   p0: wait=0, tat=1,
   p2: wait=1, tat=3,
   p1: wait=3, tat=8,
   avgWait= (0+1+3)/3=1.333..., TAT= (1+3+8)/3=4, resp= same as wait =>1.333..., preempt=0
*/
static bool test_sjf_impl(void){
    os_init();
    process_t p[3];
    init_process(&p[0],1,1,0);
    init_process(&p[1],5,1,0);
    init_process(&p[2],2,1,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    return check_stats(&rep, 1.3333, 4.0, 1.3333, 0ULL, 0.01);
}
TEST(test_sjf){
    if(!test_sjf_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_sjf => mismatch => expected W~1.33, T=4, R~1.33, pre=0");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_SJF);
    return true;
}

/* STRF => partial => 2 procs => p0=4, p1=3 =>
   We expect at least 1 preemption if quantum=2.
   We'll just check total_procs=2, preemptions>0.
*/
static bool test_strf_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0],4,1,0);
    init_process(&p[1],3,1,0);

    scheduler_select_algorithm(ALG_STRF);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=2) return false;
    if(rep.preemptions<1) return false;
    return true;
}
TEST(test_strf){
    if(!test_strf_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_strf => mismatch => expect total=2, preempt>0");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_STRF);
    return true;
}

/* HRRN => 3 procs => p0=2, p1=3, p2=4 => no preempt =>
   total_procs=3, preempt=0
*/
static bool test_hrrn_impl(void){
    os_init();
    process_t p[3];
    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);
    init_process(&p[2],4,1,0);

    scheduler_select_algorithm(ALG_HRRN);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=3) return false;
    if(rep.preemptions!=0) return false;
    return true;
}
TEST(test_hrrn){
    if(!test_hrrn_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_hrrn => mismatch => expect procs=3, preempt=0");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN);
    return true;
}

/* HRRN-RT => partial preempt => 2 procs => p0=3, p1=4 => preempt>0. */
static bool test_hrrn_rt_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,0);
    init_process(&p[1],4,1,0);

    scheduler_select_algorithm(ALG_HRRN_RT);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=2) return false;
    if(rep.preemptions<1)  return false;
    return true;
}
TEST(test_hrrn_rt){
    if(!test_hrrn_rt_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_hrrn_rt => mismatch => expect procs=2, preempt>0");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HRRN_RT);
    return true;
}

/* PRIORITY => 3 procs => p0=2,prio=3, p1=2,prio=1, p2=2,prio=2 =>
   order => p1->p2->p0 =>
   p1: wait=0,tat=2
   p2: wait=2,tat=4
   p0: wait=4,tat=6
   avgWait=2, avgTat=4, avgResp=2, pre=0
*/
static bool test_prio_impl(void){
    os_init();
    process_t p[3];
    init_process(&p[0],2,3,0);
    init_process(&p[1],2,1,0);
    init_process(&p[2],2,2,0);

    scheduler_select_algorithm(ALG_PRIORITY);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    return check_stats(&rep,2.0,4.0,2.0,0ULL,0.001);
}
TEST(test_prio){
    if(!test_prio_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_prio => mismatch => expected W=2,T=4,R=2, pre=0");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_PRIORITY);
    return true;
}

/* CFS-SRTF => 3 procs => partial => we expect preempt>0, total=3. */
static bool test_cfs_srtf_impl(void){
    os_init();
    process_t p[3];
    init_process(&p[0],2,1,0);
    init_process(&p[1],4,1,0);
    init_process(&p[2],6,1,0);

    scheduler_select_algorithm(ALG_CFS_SRTF);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=3) return false;
    if(rep.preemptions<1) return false;
    return true;
}
TEST(test_cfs_srtf){
    if(!test_cfs_srtf_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_cfs_srtf => mismatch => expect procs=3, preempt>0");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS_SRTF);
    return true;
}

/* SJF strict => 2 procs => p0=2, p1=5 =>
   wait p0=0,tat=2 => p1=2,tat=7 =>
   avg wait=1, tat=4.5 => resp=1 => pre=0
*/
static bool test_sjf_strict_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0],2,10,0);
    init_process(&p[1],5,20,0);

    scheduler_select_algorithm(ALG_SJF);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    return check_stats(&rep,1.0,4.5,1.0,0ULL,0.001);
}
TEST(test_sjf_strict){
    if(!test_sjf_strict_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_sjf_strict => mismatch => W=1,T=4.5,R=1, pre=0");
        return false;
    }
    return true;
}

void run_normal_tests(int* total,int* passed){
    tests_run=0;
    tests_failed=0;

    RUN_TEST(test_sjf);
    RUN_TEST(test_strf);
    RUN_TEST(test_hrrn);
    RUN_TEST(test_hrrn_rt);
    RUN_TEST(test_prio);
    RUN_TEST(test_cfs_srtf);
    RUN_TEST(test_sjf_strict);

    *total=tests_run;
    *passed=tests_run - tests_failed;
}
