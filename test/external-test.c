#include "external-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include "../src/runner.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int tests_run=0;
static int tests_failed=0;
extern char g_test_fail_reason[256];

/* HPC overshadow => expect 0 stats. */
bool test_external_hpc(void)
{
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=0){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_external_hpc => expected total_procs=0, got %llu",
                 (unsigned long long)rep.total_procs);
        return false;
    }
    return true;
}

/* BFS => partial => expect at least 1 preemption for 2 short procs. */
bool test_external_bfs(void)
{
    os_init();
    process_t p[2];
    init_process(&p[0],3,1,0);
    init_process(&p[1],3,1,0);

    scheduler_select_algorithm(ALG_BFS);
    scheduler_run(p,2);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=2 || rep.preemptions<1){
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_external_bfs => mismatch => procs=%llu, preempt=%llu",
                 (unsigned long long)rep.total_procs,
                 (unsigned long long)rep.preemptions);
        return false;
    }
    return true;
}

/* run shell concurrency => pass if no crash */
bool test_run_shell_concurrency(void)
{
    int count=2;
    char* lines[2];
    lines[0] = "sleep 2";
    lines[1] = "sleep 3";

    /* single core, FIFO mode */
    run_shell_commands_concurrently(count, lines, 1, ALG_FIFO, 0);
    return true;
}

void run_external_tests(void)
{
    printf("[External] => Starting external tests.\n");
    {
        tests_run++;
        bool ok=test_external_hpc();
        if(!ok){
            tests_failed++;
            printf("  FAIL: test_external_hpc => %s\n", g_test_fail_reason);
        } else {
            printf("  PASS: test_external_hpc\n");
        }
    }

    {
        tests_run++;
        bool ok=test_external_bfs();
        if(!ok){
            tests_failed++;
            printf("  FAIL: test_external_bfs => %s\n", g_test_fail_reason);
        } else {
            printf("  PASS: test_external_bfs\n");
        }
    }

    {
        tests_run++;
        bool ok=test_run_shell_concurrency();
        if(!ok){
            tests_failed++;
            printf("  FAIL: test_run_shell_concurrency => %s\n", g_test_fail_reason);
        } else {
            printf("  PASS: test_run_shell_concurrency\n");
        }
    }

    scoreboard_update_external(tests_run, tests_run - tests_failed);
    scoreboard_save();
    printf("[External] => %d total, %d passed.\n", tests_run, (tests_run - tests_failed));
}
