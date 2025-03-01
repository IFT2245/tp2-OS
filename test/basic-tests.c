// File: basic-tests.c
// Refined test suite with clearer naming, multi-level gating, concurrency & preemption coverage.

#include "basic-tests.h"
#include "../src/worker.h"
#include "../lib/library.h"
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------
   Utility: Wait with a TIMEOUT for the child to finish
------------------------------------------------------------------------ */
static bool do_wait_with_timeout(pid_t pid, int timeout_sec, int *exit_code){
    int status;
    for(int i=0; i < timeout_sec*10; i++){  // Check 10 times/sec
        pid_t w = waitpid(pid, &status, WNOHANG);
        if(w == pid){
            if(WIFEXITED(status)){
                *exit_code = WEXITSTATUS(status);
                return true;
            } else {
                // e.g., killed by signal
                *exit_code = 1; // Fail
                return true;
            }
        }
        usleep(100000); // 0.1s
    }
    // Timed out => kill child => test fails
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    *exit_code = 1;
    return false;
}

/* ------------------------------------------------------------------------
   Subprocess test runner
   - Fork + run in child, wait with timeout, record scoreboard on success/fail
------------------------------------------------------------------------ */
static bool run_test_in_subproc(
    const char* test_name,
    bool (*test_func)(void),
    scoreboard_suite_t suite,
    int timeout_sec)
{
    if(skip_remaining_tests_requested()){
        return false; // user signaled skip
    }
    // Gate behind suite unlock threshold:
    if(!scoreboard_is_unlocked(suite)){
        log_warn("Skipping %s => suite locked (below threshold).", test_name);
        return false;
    }

    pid_t pid = fork();
    if(pid < 0){
        log_error("fork() failed => cannot run test %s", test_name);
        return false;
    }
    if(pid == 0){
        // Child: run the test function directly
        bool pass = test_func();
        _exit(pass ? 0 : 1);
    }

    int exit_code = 1;
    bool finished = do_wait_with_timeout(pid, timeout_sec, &exit_code);

    bool pass = false;
    if(!finished){
        log_error("%s => TIMEOUT => FAIL", test_name);
    } else {
        pass = (exit_code == 0);
        if(pass){
            log_info("%s PASS", test_name);
        } else {
            log_error("%s FAIL", test_name);
        }
    }

    // Update scoreboard for the suite => (t=1, p=(pass?1:0))
    int t = 1, p = (pass ? 1 : 0);
    switch(suite){
        case SUITE_BASIC:        scoreboard_update_basic(t,p);         break;
        case SUITE_NORMAL:       scoreboard_update_normal(t,p);        break;
        case SUITE_EDGE:         scoreboard_update_edge(t,p);          break;
        case SUITE_HIDDEN:       scoreboard_update_hidden(t,p);        break;
        case SUITE_WFQ:          scoreboard_update_wfq(t,p);           break;
        case SUITE_MULTI_HPC:    scoreboard_update_multi_hpc(t,p);     break;
        case SUITE_BFS:          scoreboard_update_bfs(t,p);           break;
        case SUITE_MLFQ:         scoreboard_update_mlfq(t,p);          break;
        case SUITE_PRIO_PREEMPT: scoreboard_update_prio_preempt(t,p);  break;
        case SUITE_HPC_BFS:      scoreboard_update_hpc_bfs(t,p);       break;
        default: break;
    }
    return pass;
}

/* ------------------------------------------------------------------------
   Helper: check if all processes in an array are fully done (remaining_time=0).
------------------------------------------------------------------------ */
static bool all_done(const process_t* arr, int count){
    for(int i=0; i<count; i++){
        if(arr[i].remaining_time > 0) return false;
    }
    return true;
}

/* ------------------------------------------------------------------------
   (A) BASIC TESTS (ALG_FIFO)
   Weighted heavily to encourage passing early (20% in scoreboard).
------------------------------------------------------------------------ */

/* BASIC_1: Simple FIFO with 2 processes. */
static bool test_BASIC_1_fifoTwoProcs(void){
    log_info("Running test_BASIC_1_fifoTwoProcs");
    process_t p[2];
    init_process(&p[0], 3, 5, 0, 1.0);
    init_process(&p[1], 5, 7, 2, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_FIFO, ALG_NONE, p, 2, NULL, 0, 20);

    orchestrator_run(&c, 1);
    return all_done(p, 2);
}

/* BASIC_2: FIFO with 3 processes, staggered arrival. */
static bool test_BASIC_2_fifoThreeProcsStaggered(void){
    log_info("Running test_BASIC_2_fifoThreeProcsStaggered");
    process_t p[3];
    init_process(&p[0], 2, 1, 0, 1.0);  // arrives at 0
    init_process(&p[1], 4, 2, 3, 1.0);  // arrives at time=3
    init_process(&p[2], 3, 1, 5, 1.0);  // arrives at time=5

    container_t c;
    container_init(&c, 1, 0, ALG_FIFO, ALG_NONE, p, 3, NULL, 0, 30);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (B) NORMAL TESTS (Round Robin, etc.) => 15% weight
------------------------------------------------------------------------ */

/* NORMAL_1: Simple Round Robin with 2 processes. */
static bool test_NORMAL_1_rr2Procs(void){
    log_info("Running test_NORMAL_1_rr2Procs");
    process_t p[2];
    init_process(&p[0], 4, 3, 0, 1.0);
    init_process(&p[1], 2, 2, 1, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_RR, ALG_NONE, p, 2, NULL, 0, 20);

    orchestrator_run(&c, 1);
    return all_done(p,2);
}

/* NORMAL_2: Round Robin with 3 processes, arrivals. */
static bool test_NORMAL_2_rr3ProcsStaggered(void){
    log_info("Running test_NORMAL_2_rr3ProcsStaggered");
    process_t p[3];
    init_process(&p[0], 5, 1, 0, 1.0);
    init_process(&p[1], 2, 1, 1, 1.0);
    init_process(&p[2], 3, 1, 3, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_RR, ALG_NONE, p, 3, NULL, 0, 40);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (C) EDGE TESTS (non-preemptive Priority) => 10% weight
------------------------------------------------------------------------ */

/* EDGE_1: Priority non-preemptive, 3 procs same arrival. */
static bool test_EDGE_1_priorityNonPreemptive(void){
    log_info("Running test_EDGE_1_priorityNonPreemptive");
    process_t p[3];
    init_process(&p[0], 2, 1, 0, 1.0);
    init_process(&p[1], 4, 5, 0, 1.0);
    init_process(&p[2], 2, 2, 1, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_PRIORITY, ALG_NONE, p, 3, NULL, 0, 30);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* EDGE_2: Priority non-preemptive, staggered arrivals. */
static bool test_EDGE_2_priorityNonPreemptiveStaggered(void){
    log_info("Running test_EDGE_2_priorityNonPreemptiveStaggered");
    process_t p[3];
    init_process(&p[0], 3, 1, 0, 1.0);  // arrives=0
    init_process(&p[1], 2,10, 2, 1.0);  // arrives=2
    init_process(&p[2], 5, 2, 4, 1.0);  // arrives=4

    container_t c;
    container_init(&c, 1, 0, ALG_PRIORITY, ALG_NONE, p, 3, NULL, 0, 40);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (D) HIDDEN TESTS => 10% weight (often HPC scenario with SJF or HPC)
------------------------------------------------------------------------ */

/* HIDDEN_1: HPC example => main=SJF, HPC=HPC. */
static bool test_HIDDEN_1_sjfPlusHPC(void){
    log_info("Running test_HIDDEN_1_sjfPlusHPC");
    process_t mp[2];
    init_process(&mp[0], 5, 2, 0, 1.0);
    init_process(&mp[1], 5, 1, 2, 1.0);

    process_t hp[1];
    init_process(&hp[0], 6, 1, 1, 1.0);

    container_t c;
    container_init(&c, 1, 1, ALG_SJF, ALG_HPC, mp, 2, hp, 1, 30);

    orchestrator_run(&c, 1);
    return (all_done(mp,2) && all_done(hp,1));
}

/* HIDDEN_2: HPC + SJF with more arrivals. */
static bool test_HIDDEN_2_sjfPlusHPCStaggered(void){
    log_info("Running test_HIDDEN_2_sjfPlusHPCStaggered");
    process_t mp[2];
    init_process(&mp[0], 3, 1, 0, 1.0);
    init_process(&mp[1], 7, 1, 4, 1.0);

    process_t hp[2];
    init_process(&hp[0], 4, 1, 2, 1.0);
    init_process(&hp[1], 2, 1, 2, 1.0);

    container_t c;
    container_init(&c, 1, 1, ALG_SJF, ALG_HPC, mp, 2, hp, 2, 40);

    orchestrator_run(&c, 1);
    return (all_done(mp,2) && all_done(hp,2));
}

/* ------------------------------------------------------------------------
   (E) WFQ (Weighted Fair Queueing) => 10% weight
------------------------------------------------------------------------ */

/* WFQ_1: Weighted, 3 processes. */
static bool test_WFQ_1_weightedThree(void){
    log_info("Running test_WFQ_1_weightedThree");
    process_t p[3];
    init_process(&p[0], 6, 0, 0, 2.0);
    init_process(&p[1], 4, 0, 0, 1.0);
    init_process(&p[2], 3, 0, 2, 3.0);

    container_t c;
    container_init(&c, 2, 0, ALG_WFQ, ALG_NONE, p, 3, NULL, 0, 40);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* WFQ_2: Weighted, 4 processes staggered. */
static bool test_WFQ_2_weightedFourStaggered(void){
    log_info("Running test_WFQ_2_weightedFourStaggered");
    process_t p[4];
    init_process(&p[0], 3, 0, 0, 2.0);
    init_process(&p[1], 5, 0, 2, 1.0);
    init_process(&p[2], 4, 0, 2, 3.0);
    init_process(&p[3], 2, 0, 4, 2.0);

    container_t c;
    container_init(&c, 2, 0, ALG_WFQ, ALG_NONE, p, 4, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,4);
}

/* ------------------------------------------------------------------------
   (F) MULTI_HPC: multiple HPC threads => 5% weight
------------------------------------------------------------------------ */

/* MULTI_HPC_1: HPC concurrency with 2 main, 2 HPC. */
static bool test_MULTI_HPC_1_parallel(void){
    log_info("Running test_MULTI_HPC_1_parallel");
    process_t mp[2];
    init_process(&mp[0], 5, 2, 0, 1.0);
    init_process(&mp[1], 5, 1, 0, 1.0);

    process_t hp[3];
    init_process(&hp[0], 3, 2, 0, 2.0);
    init_process(&hp[1], 4, 2, 1, 1.0);
    init_process(&hp[2], 5, 1, 2, 1.5);

    container_t c;
    container_init(&c, 2, 2, ALG_RR, ALG_HPC, mp, 2, hp, 3, 50);

    orchestrator_run(&c, 1);
    return (all_done(mp,2) && all_done(hp,3));
}

/* MULTI_HPC_2: HPC concurrency, arrivals. */
static bool test_MULTI_HPC_2_parallelStaggered(void){
    log_info("Running test_MULTI_HPC_2_parallelStaggered");
    process_t mp[2];
    init_process(&mp[0], 4, 1, 0, 1.0);
    init_process(&mp[1], 6, 2, 3, 1.0);

    process_t hp[2];
    init_process(&hp[0], 4, 1, 1, 1.0);
    init_process(&hp[1], 6, 1, 2, 1.0);

    container_t c;
    container_init(&c, 2, 2, ALG_RR, ALG_HPC, mp, 2, hp, 2, 60);

    orchestrator_run(&c, 1);
    return (all_done(mp,2) && all_done(hp,2));
}

/* ------------------------------------------------------------------------
   (G) BFS scheduling => 15% weight
------------------------------------------------------------------------ */

/* BFS_1: BFS scheduling, single core. */
static bool test_BFS_1_scheduling(void){
    log_info("Running test_BFS_1_scheduling");
    process_t p[3];
    init_process(&p[0], 3, 0, 0, 1.0);
    init_process(&p[1], 8, 0, 0, 1.0);
    init_process(&p[2], 6, 0, 3, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_BFS, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* BFS_2: BFS scheduling, 2 CPU cores, arrivals. */
static bool test_BFS_2_schedulingMultiCore(void){
    log_info("Running test_BFS_2_schedulingMultiCore");
    process_t p[3];
    init_process(&p[0], 4, 0, 0, 1.0);
    init_process(&p[1], 5, 0, 2, 1.0);
    init_process(&p[2], 3, 0, 3, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_BFS, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (H) MLFQ scheduling => 5% weight
------------------------------------------------------------------------ */

/* MLFQ_1: 2 cores, processes w/ arrival offsets. */
static bool test_MLFQ_1_scheduling(void){
    log_info("Running test_MLFQ_1_scheduling");
    process_t p[3];
    init_process(&p[0], 10, 0, 0, 1.0);
    init_process(&p[1], 5,  0, 0, 1.0);
    init_process(&p[2], 7,  0, 3, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_MLFQ, ALG_NONE, p, 3, NULL, 0, 80);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* MLFQ_2: Single core MLFQ. */
static bool test_MLFQ_2_schedulingStaggered(void){
    log_info("Running test_MLFQ_2_schedulingStaggered");
    process_t p[3];
    init_process(&p[0], 6, 0, 0, 1.0);
    init_process(&p[1], 6, 0, 2, 1.0);
    init_process(&p[2], 4, 0, 4, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_MLFQ, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (I) PRIO_PREEMPT scheduling => 5% weight
------------------------------------------------------------------------ */

/* PRIO_PREEMPT_1: Preemptive Priority. */
static bool test_PRIO_PREEMPT_1_preemptive(void){
    log_info("Running test_PRIO_PREEMPT_1_preemptive");
    process_t p[3];
    init_process(&p[0], 8, 5, 0, 1.0);
    init_process(&p[1], 3, 1, 3, 1.0);
    init_process(&p[2], 2,10, 2, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_PRIO_PREEMPT, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* PRIO_PREEMPT_2: Preemptive Priority with arrivals. */
static bool test_PRIO_PREEMPT_2_preemptiveStaggered(void){
    log_info("Running test_PRIO_PREEMPT_2_preemptiveStaggered");
    process_t p[3];
    // arrives=0, priority=8
    init_process(&p[0], 5, 8, 0, 1.0);
    // arrives=2, priority=2
    init_process(&p[1], 3, 2, 2, 1.0);
    // arrives=4, priority=1 => highest
    init_process(&p[2], 4, 1, 4, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_PRIO_PREEMPT, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (J) BONUS HPC_BFS tests => 5% weight + HPC bonus if scoreboard_set_sc_hpc(1)
------------------------------------------------------------------------ */

/* HPC_BFS_1: HPC BFS test, 0 main cores => HPC can steal main procs. */
static bool test_BONUS_1_hpc_bfs(void){
    log_info("Running test_BONUS_1_hpc_bfs");
    process_t mainP[1];
    init_process(&mainP[0], 4, 0, 0, 1.0);

    process_t hpcP[2];
    init_process(&hpcP[0], 3, 0, 1, 1.0);
    init_process(&hpcP[1], 4, 0, 2, 1.0);

    container_t c;
    container_init(&c, 0, 2, ALG_NONE, ALG_BFS, mainP, 1, hpcP, 2, 40);

    orchestrator_run(&c, 1);
    return (all_done(mainP,1) && all_done(hpcP,2));
}

/* HPC_BFS_2: HPC BFS test with 2 HPC threads. */
static bool test_BONUS_2_hpc_bfsStaggered(void){
    log_info("Running test_BONUS_2_hpc_bfsStaggered");
    process_t mainP[2];
    init_process(&mainP[0], 5, 0, 0, 1.0);
    init_process(&mainP[1], 2, 0, 2, 1.0);

    process_t hpcP[1];
    init_process(&hpcP[0], 4, 0, 3, 1.0);

    container_t c;
    container_init(&c, 0, 2, ALG_NONE, ALG_BFS, mainP, 2, hpcP, 1, 40);

    orchestrator_run(&c, 1);
    return (all_done(mainP,2) && all_done(hpcP,1));
}

/* ------------------------------------------------------------------------
   run_all_tests():
   - We call each test in order, each locked behind scoreboard gating
     so we only “unlock” the next suite at 60% pass in the previous.
   - HPC_BFS bonus tests run only if set_bonus_test(1) was called.
------------------------------------------------------------------------ */
#define DEFAULT_TEST_TIMEOUT_SEC 5

typedef struct {
    const char*       name;
    bool            (*impl)(void);
    scoreboard_suite_t suite;
} TestCase;

static bool execute_single_test(const TestCase* test, int timeout_sec){
    return run_test_in_subproc(test->name, test->impl, test->suite, timeout_sec);
}

void run_all_tests(void)
{
    // We set 'look_remaining_tests' so SIGTERM can skip concurrency tests if needed
    set_look_remaining_tests(1);

    // Check if HPC_BFS was originally enabled (bonus test)
    bool bonus_enabled = (is_bonus_test() == 1);
    // Temporarily disable if needed, then restore
    set_bonus_test(0);

    // Here are all the standard tests in order (grouped by suite):
    const TestCase test_cases[] = {
        // (A) BASIC
        {"test_BASIC_1_fifoTwoProcs",            test_BASIC_1_fifoTwoProcs,            SUITE_BASIC},
        {"test_BASIC_2_fifoThreeProcsStaggered", test_BASIC_2_fifoThreeProcsStaggered, SUITE_BASIC},

        // (B) NORMAL
        {"test_NORMAL_1_rr2Procs",        test_NORMAL_1_rr2Procs,        SUITE_NORMAL},
        {"test_NORMAL_2_rr3ProcsStaggered", test_NORMAL_2_rr3ProcsStaggered, SUITE_NORMAL},

        // (C) EDGE
        {"test_EDGE_1_priorityNonPreemptive",           test_EDGE_1_priorityNonPreemptive,           SUITE_EDGE},
        {"test_EDGE_2_priorityNonPreemptiveStaggered",  test_EDGE_2_priorityNonPreemptiveStaggered,  SUITE_EDGE},

        // (D) HIDDEN
        {"test_HIDDEN_1_sjfPlusHPC",          test_HIDDEN_1_sjfPlusHPC,          SUITE_HIDDEN},
        {"test_HIDDEN_2_sjfPlusHPCStaggered", test_HIDDEN_2_sjfPlusHPCStaggered, SUITE_HIDDEN},

        // (E) WFQ
        {"test_WFQ_1_weightedThree",     test_WFQ_1_weightedThree,     SUITE_WFQ},
        {"test_WFQ_2_weightedFourStaggered", test_WFQ_2_weightedFourStaggered, SUITE_WFQ},

        // (F) MULTI_HPC
        {"test_MULTI_HPC_1_parallel",       test_MULTI_HPC_1_parallel,       SUITE_MULTI_HPC},
        {"test_MULTI_HPC_2_parallelStaggered", test_MULTI_HPC_2_parallelStaggered, SUITE_MULTI_HPC},

        // (G) BFS
        {"test_BFS_1_scheduling",        test_BFS_1_scheduling,        SUITE_BFS},
        {"test_BFS_2_schedulingMultiCore", test_BFS_2_schedulingMultiCore, SUITE_BFS},

        // (H) MLFQ
        {"test_MLFQ_1_scheduling",              test_MLFQ_1_scheduling,              SUITE_MLFQ},
        {"test_MLFQ_2_schedulingStaggered",     test_MLFQ_2_schedulingStaggered,     SUITE_MLFQ},

        // (I) PRIO_PREEMPT
        {"test_PRIO_PREEMPT_1_preemptive",           test_PRIO_PREEMPT_1_preemptive,           SUITE_PRIO_PREEMPT},
        {"test_PRIO_PREEMPT_2_preemptiveStaggered",  test_PRIO_PREEMPT_2_preemptiveStaggered,  SUITE_PRIO_PREEMPT},
    };

    // Run each standard test in turn
    for(size_t i=0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++){
        if(!execute_single_test(&test_cases[i], DEFAULT_TEST_TIMEOUT_SEC)){
            if(skip_remaining_tests_requested()) {
                // user signaled skip => break out
                return;
            }
        }
    }

    // Possibly restore HPC_BFS bonus tests if originally enabled
    if(bonus_enabled){
        set_bonus_test(1); // re-enable user’s bonus HPC BFS
        const TestCase bonus_tests[] = {
            {"test_BONUS_1_hpc_bfs",           test_BONUS_1_hpc_bfs,           SUITE_HPC_BFS},
            {"test_BONUS_2_hpc_bfsStaggered",  test_BONUS_2_hpc_bfsStaggered,  SUITE_HPC_BFS}
        };
        for(size_t i=0; i<2; i++){
            execute_single_test(&bonus_tests[i], DEFAULT_TEST_TIMEOUT_SEC);
        }
    }

    set_look_remaining_tests(0);

}

