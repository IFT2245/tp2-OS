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
    for(int i=0; i < timeout_sec*10; i++){
        pid_t w = waitpid(pid, &status, WNOHANG);
        if(w == pid){
            if(WIFEXITED(status)){
                *exit_code = WEXITSTATUS(status);
                return true;
            } else {
                /* e.g., killed by signal */
                *exit_code = 1; /* fail */
                return true;
            }
        }
        usleep(100000); /* 0.1s */
    }
    /* Timed out => kill child => test fail */
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    *exit_code = 1;
    return false;
}

/* ------------------------------------------------------------------------
   Subprocess test runner
------------------------------------------------------------------------ */
static bool run_test_in_subproc(
    const char* test_name,
    bool (*test_func)(void),
    scoreboard_suite_t suite,
    int timeout_sec)
{
    if(skip_remaining_tests_requested()){
        return false;
    }
    pid_t pid = fork();
    if(pid < 0){
        log_error("fork() failed => cannot run test %s", test_name);
        return false;
    }
    if(pid == 0){
        /* Child: just run the test function. */
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

    /* Update scoreboard for this suite => (t=1, p=(pass?1:0)) */
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

/* Helper: check if all given processes are done. */
static bool all_done(const process_t* arr, int count){
    for(int i=0;i<count;i++){
        if(arr[i].remaining_time>0) return false;
    }
    return true;
}

/* ------------------------------------------------------------------------
   (A) BASIC TESTS (FIFO)
------------------------------------------------------------------------ */

/* 1) BASIC test #1: FIFO simple 2 procs. */
static bool test_BASIC_fifo(void){
    log_info("Running test_BASIC_fifo");
    process_t p[2];
    init_process(&p[0], 3, 5, 0, 1.0);
    init_process(&p[1], 5, 7, 2, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_FIFO, ALG_NONE, p, 2, NULL, 0, 20);

    orchestrator_run(&c, 1);
    return all_done(p, 2);
}

/* 2) BASIC test #2: FIFO with 3 processes arriving at different times. */
static bool test_BASIC_fifo2(void){
    log_info("Running test_BASIC_fifo2");
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
   (B) NORMAL TESTS (Round Robin, etc.)
------------------------------------------------------------------------ */

/* 1) NORMAL test #1: Round Robin. */
static bool test_NORMAL_rr(void){
    log_info("Running test_NORMAL_rr");
    process_t p[2];
    init_process(&p[0], 4, 3, 0, 1.0);
    init_process(&p[1], 2, 2, 1, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_RR, ALG_NONE, p, 2, NULL, 0, 20);

    orchestrator_run(&c, 1);
    return all_done(p,2);
}

/* 2) NORMAL test #2: Round Robin with 3 processes, different arrival times. */
static bool test_NORMAL_rr2(void){
    log_info("Running test_NORMAL_rr2");
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
   (C) EDGE TESTS (non-preemptive Priority)
------------------------------------------------------------------------ */

/* 1) EDGE test #1: Priority non-preemptive. */
static bool test_EDGE_priority(void){
    log_info("Running test_EDGE_priority");
    process_t p[3];
    init_process(&p[0], 2, 1, 0, 1.0);
    init_process(&p[1], 4, 5, 0, 1.0);
    init_process(&p[2], 2, 2, 1, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_PRIORITY, ALG_NONE, p, 3, NULL, 0, 30);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 2) EDGE test #2: Priority non-preemptive with arrival offsets. */
static bool test_EDGE_priority2(void){
    log_info("Running test_EDGE_priority2");
    process_t p[3];
    init_process(&p[0], 3, 1, 0, 1.0);  // priority=1, arrives=0
    init_process(&p[1], 2, 10, 2, 1.0); // priority=10, arrives=2
    init_process(&p[2], 5, 2, 4, 1.0);  // priority=2, arrives=4

    container_t c;
    container_init(&c, 1, 0, ALG_PRIORITY, ALG_NONE, p, 3, NULL, 0, 40);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (D) HIDDEN TESTS (Example HPC usage, with SJF or HPC)
------------------------------------------------------------------------ */

/* 1) HIDDEN test #1: HPC example => main= SJF, HPC= HPC. */
static bool test_HIDDEN_sjf_hpc(void){
    log_info("Running test_HIDDEN_sjf_hpc");
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

/* 2) HIDDEN test #2: HPC SJF vs HPC arrival. */
static bool test_HIDDEN_sjf_hpc2(void){
    log_info("Running test_HIDDEN_sjf_hpc2");
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
   (E) WFQ (Weighted Fair Queueing)
------------------------------------------------------------------------ */

/* 1) WFQ test #1: Weighted. */
static bool test_WFQ_weighted(void){
    log_info("Running test_WFQ_weighted");
    process_t p[3];
    init_process(&p[0], 6, 0, 0, 2.0);
    init_process(&p[1], 4, 0, 0, 1.0);
    init_process(&p[2], 3, 0, 2, 3.0);

    container_t c;
    container_init(&c, 2, 0, ALG_WFQ, ALG_NONE, p, 3, NULL, 0, 40);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 2) WFQ test #2: Weighted with more processes & staggered arrivals. */
static bool test_WFQ_weighted2(void){
    log_info("Running test_WFQ_weighted2");
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
   (F) MULTI_HPC: multiple HPC threads
------------------------------------------------------------------------ */

/* 1) MULTI_HPC test #1. */
static bool test_MULTI_HPC_parallel(void){
    log_info("Running test_MULTI_HPC_parallel");
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

/* 2) MULTI_HPC test #2: slightly different HPC threads & arrivals. */
static bool test_MULTI_HPC_parallel2(void){
    log_info("Running test_MULTI_HPC_parallel2");
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
   (G) BFS scheduling
------------------------------------------------------------------------ */

/* 1) BFS test #1. */
static bool test_BFS_scheduling(void){
    log_info("Running test_BFS_scheduling");
    process_t p[3];
    init_process(&p[0], 3, 0, 0, 1.0);
    init_process(&p[1], 8, 0, 0, 1.0);
    init_process(&p[2], 6, 0, 3, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_BFS, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 2) BFS test #2: BFS with 2 CPU cores & arrivals. */
static bool test_BFS_scheduling2(void){
    log_info("Running test_BFS_scheduling2");
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
   (H) MLFQ scheduling
------------------------------------------------------------------------ */

/* 1) MLFQ test #1. */
static bool test_MLFQ_scheduling(void){
    log_info("Running test_MLFQ_scheduling");
    process_t p[3];
    init_process(&p[0], 10, 0, 0, 1.0);
    init_process(&p[1], 5, 0, 0, 1.0);
    init_process(&p[2], 7, 0, 3, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_MLFQ, ALG_NONE, p, 3, NULL, 0, 80);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 2) MLFQ test #2. */
static bool test_MLFQ_scheduling2(void){
    log_info("Running test_MLFQ_scheduling2");
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
   (I) PRIO_PREEMPT scheduling
------------------------------------------------------------------------ */

/* 1) PRIO_PREEMPT #1: Preemptive Priority scheduling. */
static bool test_PRIO_PREEMPT_scheduling(void){
    log_info("Running test_PRIO_PREEMPT_scheduling");
    process_t p[3];
    init_process(&p[0], 8, 5, 0, 1.0);
    init_process(&p[1], 3, 1, 3, 1.0);
    init_process(&p[2], 2,10, 2, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_PRIO_PREEMPT, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 2) PRIO_PREEMPT #2: Arrival times and changing priorities. */
static bool test_PRIO_PREEMPT_scheduling2(void){
    log_info("Running test_PRIO_PREEMPT_scheduling2");
    process_t p[3];
    init_process(&p[0], 5, 8, 0, 1.0); // arrives=0, priority=8
    init_process(&p[1], 3, 2, 2, 1.0); // arrives=2, priority=2
    init_process(&p[2], 4, 1, 4, 1.0); // arrives=4, priority=1 => highest

    container_t c;
    container_init(&c, 1, 0, ALG_PRIO_PREEMPT, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* ------------------------------------------------------------------------
   (J) BONUS HPC_BFS tests
   If HPC bonus is toggled (set_bonus_test(1)), we run them.
------------------------------------------------------------------------ */

/* HPC_BFS test #1 => HPC BFS test if user toggles bonus.
   0 main cores => HPC can steal main processes. */
static bool test_BONUS_hpc_bfs(void){
    log_info("Running test_BONUS_hpc_bfs");
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

/* HPC_BFS test #2 => Another BFS HPC scenario with 2 HPC threads. */
static bool test_BONUS_hpc_bfs2(void){
    log_info("Running test_BONUS_hpc_bfs2");
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
   run_all_tests()
   We group each level's tests, check scoreboard gating,
   and run them in subprocs with 5s timeouts.
------------------------------------------------------------------------ */
#define DEFAULT_TEST_TIMEOUT_SEC 5

typedef struct {
    const char* name;
    bool (*impl)(void);
    scoreboard_suite_t suite;
} TestCase;

static bool execute_single_test(const TestCase* test, int timeout_sec){
    if(skip_remaining_tests_requested()){
        return false;
    }
    /* Check if suite is locked behind threshold: */
    if(!scoreboard_is_unlocked(test->suite)){
        log_warn("Skipping %s => suite locked (below threshold).", test->name);
        return false;
    }
    return run_test_in_subproc(test->name, test->impl, test->suite, timeout_sec);
}

void run_all_tests(void)
{
    set_look_remaining_tests(1);

    /* Temporarily disable HPC BFS bonus if not set yet: */
    bool was_bonus_enabled = (is_bonus_test() == 1);
    set_bonus_test(0);

    /* Listing all refined tests: */
    const TestCase test_cases[] = {
        // (A) BASIC
        {"test_BASIC_fifo",    test_BASIC_fifo,    SUITE_BASIC},
        {"test_BASIC_fifo2",   test_BASIC_fifo2,   SUITE_BASIC},

        // (B) NORMAL
        {"test_NORMAL_rr",     test_NORMAL_rr,     SUITE_NORMAL},
        {"test_NORMAL_rr2",    test_NORMAL_rr2,    SUITE_NORMAL},

        // (C) EDGE
        {"test_EDGE_priority",  test_EDGE_priority,  SUITE_EDGE},
        {"test_EDGE_priority2", test_EDGE_priority2, SUITE_EDGE},

        // (D) HIDDEN
        {"test_HIDDEN_sjf_hpc",  test_HIDDEN_sjf_hpc,  SUITE_HIDDEN},
        {"test_HIDDEN_sjf_hpc2", test_HIDDEN_sjf_hpc2, SUITE_HIDDEN},

        // (E) WFQ
        {"test_WFQ_weighted",   test_WFQ_weighted,   SUITE_WFQ},
        {"test_WFQ_weighted2",  test_WFQ_weighted2,  SUITE_WFQ},

        // (F) MULTI_HPC
        {"test_MULTI_HPC_parallel",  test_MULTI_HPC_parallel,  SUITE_MULTI_HPC},
        {"test_MULTI_HPC_parallel2", test_MULTI_HPC_parallel2, SUITE_MULTI_HPC},

        // (G) BFS
        {"test_BFS_scheduling",  test_BFS_scheduling,  SUITE_BFS},
        {"test_BFS_scheduling2", test_BFS_scheduling2, SUITE_BFS},

        // (H) MLFQ
        {"test_MLFQ_scheduling",  test_MLFQ_scheduling,  SUITE_MLFQ},
        {"test_MLFQ_scheduling2", test_MLFQ_scheduling2, SUITE_MLFQ},

        // (I) PRIO_PREEMPT
        {"test_PRIO_PREEMPT_scheduling",  test_PRIO_PREEMPT_scheduling,  SUITE_PRIO_PREEMPT},
        {"test_PRIO_PREEMPT_scheduling2", test_PRIO_PREEMPT_scheduling2, SUITE_PRIO_PREEMPT},
    };

    /* Run each standard test in turn */
    for(size_t i=0; i<sizeof(test_cases)/sizeof(test_cases[0]); i++){
        if(!execute_single_test(&test_cases[i], DEFAULT_TEST_TIMEOUT_SEC)){
            if(skip_remaining_tests_requested()){
                return;
            }
        }
    }

    /* Possibly restore HPC BFS bonus tests if it was enabled: */
    if(was_bonus_enabled){
        set_bonus_test(1);
        /* We have two HPC_BFS tests: */
        const TestCase bonus_tests[] = {
            {"test_BONUS_hpc_bfs",  test_BONUS_hpc_bfs,  SUITE_HPC_BFS},
            {"test_BONUS_hpc_bfs2", test_BONUS_hpc_bfs2, SUITE_HPC_BFS}
        };
        for(size_t i=0; i<2; i++){
            execute_single_test(&bonus_tests[i], DEFAULT_TEST_TIMEOUT_SEC);
        }
    }
}
