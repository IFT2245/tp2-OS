#include "tests.h"


/*
  Each test is run in a child process with a TIMEOUT
  to avoid indefinite blocking. If the child times out,
  we kill it => test FAIL => move on.
*/

/* A small helper to wait up to N seconds for the child to finish. */
static bool do_wait_with_timeout(pid_t pid, int timeout_sec, int *exit_code){
    int status;
    for(int i=0; i < timeout_sec*10; i++){
        pid_t w = waitpid(pid, &status, WNOHANG);
        if(w == pid){
            if(WIFEXITED(status)){
                *exit_code = WEXITSTATUS(status);
                return true;
            } else {
                /* e.g. if child was killed by signal */
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

/**
 * @brief Helper to run test_func() in a child with a timeout.
 *
 * @param test_name    name of the test (for logging)
 * @param test_func    function returning `bool` (true=pass, false=fail)
 * @param suite        scoreboard suite enum
 * @param timeout_sec  kill after these many seconds
 * @return true if pass, false if fail
 */
static bool run_test_in_subproc(
    const char* test_name,
    bool (*test_func)(void),
    scoreboard_suite_t suite,
    int timeout_sec)
{
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

    /* Parent: wait up to timeout_sec for child to exit. */
    int exit_code = 1;
    bool finished = do_wait_with_timeout(pid, timeout_sec, &exit_code);

    bool pass = false;
    if(!finished){
        /* We had to kill it => TIMEOUT => fail. */
        log_error("%s => TIMEOUT => FAIL", test_name);
    } else {
        pass = (exit_code == 0);
        if(pass){
            log_info("%s PASS", test_name);
        } else {
            log_error("%s FAIL", test_name);
        }
    }

    /* Update scoreboard for this suite.
       We consider 1 test, pass or fail => (t=1, p=(pass?1:0)). */
    int t = 1, p = pass ? 1 : 0;
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
   TEST IMPLEMENTATIONS (child side).
   They each return bool: true => pass, false => fail.

   We show them in a simpler manner.
   Some only check that the processes finish.
   Some can do extra timeline checks if you wish.
------------------------------------------------------------------------ */

/* 1) Basic FIFO test. */
static bool test_basic_fifo_impl(void){
    log_info("Running test_basic_fifo");
    process_t p[2];
    init_process(&p[0], 3, 5, 0, 1.0);
    init_process(&p[1], 5, 7, 2, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_FIFO, ALG_NONE, p, 2, NULL, 0, 20);

    /* Run single container. */
    orchestrator_run(&c, 1);

    /* Check if all done. */
    return all_done(p, 2);
}

/* 2) Round Robin normal test. */
static bool test_normal_rr_impl(void){
    log_info("Running test_normal_rr");
    process_t p[2];
    init_process(&p[0], 4, 3, 0, 1.0);
    init_process(&p[1], 2, 2, 1, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_RR, ALG_NONE, p, 2, NULL, 0, 20);

    orchestrator_run(&c, 1);
    return all_done(p,2);
}

/* 3) Priority scheduling (non-preemptive). */
static bool test_edge_priority_impl(void){
    log_info("Running test_edge_priority");
    process_t p[3];
    init_process(&p[0], 2, 1, 0, 1.0);
    init_process(&p[1], 4, 5, 0, 1.0);
    init_process(&p[2], 2, 2, 1, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_PRIORITY, ALG_NONE, p, 3, NULL, 0, 30);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 4) HPC hidden test => SJF + HPC. */
static bool test_hidden_hpc_impl(void){
    log_info("Running test_hidden_hpc");
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

/* 5) Weighted Fair Queueing test. */
static bool test_wfq_impl(void){
    log_info("Running test_wfq");
    process_t p[3];
    init_process(&p[0], 6, 0, 0, 2.0);
    init_process(&p[1], 4, 0, 0, 1.0);
    init_process(&p[2], 3, 0, 2, 3.0);

    container_t c;
    container_init(&c, 2, 0, ALG_WFQ, ALG_NONE, p, 3, NULL, 0, 40);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 6) Multiple HPC threads. */
static bool test_multi_hpc_impl(void){
    log_info("Running test_multi_hpc");
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

/* 7) BFS scheduling. */
static bool test_bfs_scheduling_impl(void){
    log_info("Running test_bfs_scheduling");
    process_t p[3];
    init_process(&p[0], 3, 0, 0, 1.0);
    init_process(&p[1], 8, 0, 0, 1.0);
    init_process(&p[2], 6, 0, 2, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_BFS, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 8) MLFQ scheduling. */
static bool test_mlfq_scheduling_impl(void){
    log_info("Running test_mlfq_scheduling");
    process_t p[3];
    init_process(&p[0], 10, 0, 0, 1.0);
    init_process(&p[1], 5, 0, 0, 1.0);
    init_process(&p[2], 7, 0, 3, 1.0);

    container_t c;
    container_init(&c, 2, 0, ALG_MLFQ, ALG_NONE, p, 3, NULL, 0, 80);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 9) Preemptive Priority scheduling. */
static bool test_preemptive_priority_impl(void){
    log_info("Running test_preemptive_priority");
    process_t p[3];
    init_process(&p[0], 8, 5, 0, 1.0);
    init_process(&p[1], 3, 1, 3, 1.0);
    init_process(&p[2], 2,10, 2, 1.0);

    container_t c;
    container_init(&c, 1, 0, ALG_PRIO_PREEMPT, ALG_NONE, p, 3, NULL, 0, 50);

    orchestrator_run(&c, 1);
    return all_done(p,3);
}

/* 10) HPC BFS test => HPC with BFS, 0 main cores + main processes => HPC steals them. */
static bool test_hpc_bfs_impl(void){
    log_info("Running test_hpc_bfs");
    process_t mainP[1];
    init_process(&mainP[0], 4, 0, 0, 1.0);

    process_t hpcP[2];
    init_process(&hpcP[0], 3, 0, 1, 1.0);
    init_process(&hpcP[1], 4, 0, 2, 1.0);

    container_t c;
    /* 0 main cores, 2 HPC threads => BFS on HPC side.
       container_init sees there's 1 main process but 0 cores => c->allow_hpc_steal=true
       => HPC BFS threads can run that main process.
    */
    container_init(&c, 0, 2, ALG_NONE, ALG_BFS, mainP, 1, hpcP, 2, 40);

    orchestrator_run(&c, 1);

    return (all_done(mainP,1) && all_done(hpcP,2));
}

/* ------------------------------------------------------------------------
   run_all_tests()
   Launch each test in a subproc with 5s timeout.
------------------------------------------------------------------------ */
void run_all_tests(void){
    run_test_in_subproc("test_basic_fifo",          test_basic_fifo_impl,         SUITE_BASIC,       5);
    run_test_in_subproc("test_normal_rr",           test_normal_rr_impl,          SUITE_NORMAL,      5);
    run_test_in_subproc("test_edge_priority",       test_edge_priority_impl,      SUITE_EDGE,        5);
    run_test_in_subproc("test_hidden_hpc",          test_hidden_hpc_impl,         SUITE_HIDDEN,      5);
    run_test_in_subproc("test_wfq",                 test_wfq_impl,                SUITE_WFQ,         5);
    run_test_in_subproc("test_multi_hpc",           test_multi_hpc_impl,          SUITE_MULTI_HPC,   5);
    run_test_in_subproc("test_bfs_scheduling",      test_bfs_scheduling_impl,     SUITE_BFS,         5);
    run_test_in_subproc("test_mlfq_scheduling",     test_mlfq_scheduling_impl,    SUITE_MLFQ,        5);
    run_test_in_subproc("test_preemptive_priority", test_preemptive_priority_impl,SUITE_PRIO_PREEMPT,5);
    run_test_in_subproc("test_hpc_bfs",             test_hpc_bfs_impl,            SUITE_HPC_BFS,     5);

    /* Each test logs pass/fail individually. The scoreboard is updated. */
}
