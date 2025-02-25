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

static int tests_run=0, tests_failed=0;
static char g_test_fail_reason[256];

/* HPC overshadow test */
static bool test_external_hpc(void) {
    tests_run++;
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=0){
        tests_failed++;
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_external_hpc => expected total_procs=0, got %llu",
                 (unsigned long long)rep.total_procs);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HPC_OVERSHADOW);
    return true;
}

/* BFS partial test. */
static bool test_external_bfs(void) {
    tests_run++;
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
        tests_failed++;
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_external_bfs => mismatch => procs=%llu, preempt=%llu",
                 (unsigned long long)rep.total_procs,
                 (unsigned long long)rep.preemptions);
        test_set_fail_reason(g_test_fail_reason);
        return false;
    }
    scoreboard_set_sc_mastered(ALG_BFS);
    return true;
}

/* Shell concurrency sample test => just run 2 commands with FIFO. */
static bool test_run_shell_concurrency(void) {
    tests_run++;
    int count=2;
    char* lines[2];
    lines[0] = "sleep 2";
    lines[1] = "sleep 3";

    run_shell_commands_concurrently(count, lines, 1, ALG_FIFO, 0);
    /* Not truly verifying output here, just ensuring it runs. */
    return true;
}

void run_external_tests(void) {
    printf("\n\033[1m\033[93m╔═════════ EXTERNAL TESTS START ═════════╗\033[0m\n");

    tests_run=0;
    tests_failed=0;

    bool ok1 = test_external_hpc();
    bool ok2 = test_external_bfs();
    bool ok3 = test_run_shell_concurrency();
    (void)ok1; (void)ok2; (void)ok3;

    /* update scoreboard after we run. */
    scoreboard_update_external(tests_run, (tests_run - tests_failed));
    scoreboard_save();

    printf("\033[1m\033[93m╔════════════════════════════════════════════╗\n");
    printf("║   EXTERNAL TESTS RESULTS: %d / %d passed     ║\n", (tests_run - tests_failed), tests_run);
    if(tests_failed>0) {
        printf("║   FAILURES => see reason above logs        ║\n");
    }
    printf("╚════════════════════════════════════════════╝\033[0m\n");
}
