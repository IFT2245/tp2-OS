#include "basic-test.h"
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

static int almost_equal(double a, double b, double eps){
    return fabs(a - b) < eps;
}
static bool check_stats(const sched_report_t* rep,
                        double exp_wait, double exp_tat,
                        double exp_resp, unsigned long long exp_preempt,
                        double eps)
{
    if(!almost_equal(rep->avg_wait,       exp_wait, eps)) return false;
    if(!almost_equal(rep->avg_turnaround, exp_tat,  eps)) return false;
    if(!almost_equal(rep->avg_response,   exp_resp, eps)) return false;
    if(rep->preemptions != exp_preempt)                  return false;
    return true;
}

extern char g_test_fail_reason[256];

/* FIFO test => 2 procs => p0=3, p1=5 => arrival=0 =>
   wait p0=0,tat=3 => p1=3,8 => avg wait=1.5, tat=5.5 => resp=1.5 => preempt=0 */
static bool test_fifo_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 1, 0);
    init_process(&p[1], 5, 1, 0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    return check_stats(&rep, 1.5, 5.5, 1.5, 0ULL, 0.001);
}
TEST(test_fifo){
    if(!test_fifo_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_fifo => FIFO stats mismatch (expected W=1.5,T=5.5,R=1.5,pre=0)");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_FIFO);
    return true;
}

/* RR => p0=2, p1=2 => quantum=2 => each runs once =>
   p0 wait=0, tat=2 => p1 wait=2, tat=4 =>
   avg wait=1, tat=3 => resp=0 for p0, 2 for p1 => average=1 => preempt=0 (since each finishes in its timeslice).
*/
static bool test_rr_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0], 2, 1, 0);
    init_process(&p[1], 2, 1, 0);

    scheduler_select_algorithm(ALG_RR);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    /* We expect wait=1, tat=3, resp=1, preempt=0. */
    return check_stats(&rep, 1.0, 3.0, 1.0, 0ULL, 0.001);
}
TEST(test_rr){
    if(!test_rr_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_rr => RR stats mismatch (expected W=1,T=3,R=1,pre=0)");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_RR);
    return true;
}

/* CFS => 2 procs => p0=3, p1=4 => no preemption =>
   p0 wait=0,tat=3 => p1 wait=3,tat=7 =>
   avg wait=1.5, tat=5.0, resp=1.5 => preempt=0 */
static bool test_cfs_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0], 3, 0, 0);
    init_process(&p[1], 4, 0, 0);

    scheduler_select_algorithm(ALG_CFS);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    return check_stats(&rep, 1.5, 5.0, 1.5, 0ULL, 0.001);
}
TEST(test_cfs){
    if(!test_cfs_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_cfs => mismatch (expected W=1.5,T=5.0,R=1.5,pre=0)");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_CFS);
    return true;
}

/* BFS => 3 procs => partial => quantum=2 => we expect preempt>0, total_procs=3.
   We won't do exact wait/tat check, just a minimal assertion. */
static bool test_bfs_impl(void){
    os_init();
    process_t p[3];
    init_process(&p[0],2,1,0);
    init_process(&p[1],3,1,0);
    init_process(&p[2],4,1,0);

    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,3);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=3) return false;
    if(rep.preemptions<1)  return false;
    return true;
}
TEST(test_bfs){
    if(!test_bfs_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_bfs => BFS mismatch => expected preempt>0, procs=3");
        return false;
    }
    scoreboard_set_sc_mastered(ALG_BFS);
    return true;
}

/* pipeline => no scheduling => pass if no crash. */
static bool test_pipeline_impl(void){
    os_init();
    os_pipeline_example();
    os_cleanup();
    return true;
}
TEST(test_pipeline){
    if(!test_pipeline_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_pipeline => unexpected fail");
        return false;
    }
    return true;
}

/* distributed => pass if no crash. */
static bool test_distributed_impl(void){
    os_init();
    os_run_distributed_example();
    os_cleanup();
    return true;
}
TEST(test_distributed){
    if(!test_distributed_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_distributed => unexpected fail");
        return false;
    }
    return true;
}

/* FIFO strict => 2 procs => p0=3, p1=4 => same as above => W=1.5, T=5.0, R=1.5, pre=0. */
static bool test_fifo_strict_impl(void){
    os_init();
    process_t p[2];
    init_process(&p[0],3,10,0);
    init_process(&p[1],4,20,0);

    scheduler_select_algorithm(ALG_FIFO);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    return check_stats(&rep, 1.5, 5.0, 1.5, 0ULL, 0.001);
}
TEST(test_fifo_strict){
    if(!test_fifo_strict_impl()){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_fifo_strict => mismatch => W=1.5,T=5.0,R=1.5,pre=0");
        return false;
    }
    return true;
}

void run_basic_tests(int* total,int* passed){
    tests_run=0;
    tests_failed=0;

    RUN_TEST(test_fifo);
    RUN_TEST(test_rr);
    RUN_TEST(test_cfs);
    RUN_TEST(test_bfs);
    RUN_TEST(test_pipeline);
    RUN_TEST(test_distributed);
    RUN_TEST(test_fifo_strict);

    *total = tests_run;
    *passed= tests_run - tests_failed;
}
