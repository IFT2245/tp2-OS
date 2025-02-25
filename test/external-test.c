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

/* We'll track test counts. */
static int tests_run=0, tests_failed=0;
static char g_test_fail_reason[256];

/* prototypes */
static bool test_external_hpc(void);
static bool test_external_bfs(void);
static bool test_run_shell_concurrency(void);

/* Array for single test picking. */
typedef bool(*ext_test_fn)(void);
static ext_test_fn g_ext_tests[] = {
    test_external_hpc,
    test_external_bfs,
    test_run_shell_concurrency
};
static const char* g_ext_names[] = {
    "External HPC Overshadow",
    "External BFS Check",
    "Shell Concurrency"
};
static const int g_ext_count = (int)(sizeof(g_ext_tests)/sizeof(g_ext_tests[0]));

int external_test_get_count(void){
    return g_ext_count;
}
const char* external_test_get_name(int index){
    if(index<0 || index>=g_ext_count) return NULL;
    return g_ext_names[index];
}
int external_test_run_single(int index){
    if(index<0 || index>=g_ext_count) return 0;
    tests_run=0; tests_failed=0;
    bool ok = g_ext_tests[index]();
    int passcount = ok ? 1 : 0;
    scoreboard_update_external(1, passcount);
    scoreboard_save();
    return passcount;
}

/* Implementations */
static bool test_external_hpc(void)
{
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0,0,0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    tests_run++;
    if(rep.total_procs!=0){
        tests_failed++;
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_external_hpc => expected total_procs=0, got %llu",
                 (unsigned long long)rep.total_procs);
        return false;
    }
    return true;
}

static bool test_external_bfs(void)
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

    tests_run++;
    if(rep.total_procs!=2 || rep.preemptions<1){
        tests_failed++;
        snprintf(g_test_fail_reason,sizeof(g_test_fail_reason),
                 "test_external_bfs => mismatch => procs=%llu, preempt=%llu",
                 (unsigned long long)rep.total_procs,
                 (unsigned long long)rep.preemptions);
        return false;
    }
    return true;
}

static bool test_run_shell_concurrency(void)
{
    tests_run++;
    int count=2;
    char* lines[2];
    lines[0] = "sleep 2";
    lines[1] = "sleep 3";

    /* single core, FIFO mode => run concurrency => pass if no crash. */
    run_shell_commands_concurrently(count, lines, 1, ALG_FIFO, 0);
    return true;
}

void run_external_tests(void)
{
    printf("[External] => Starting external tests.\n");
    /* We'll run all 3. */
    tests_run=0;
    tests_failed=0;

    test_external_hpc();
    test_external_bfs();
    test_run_shell_concurrency();

    scoreboard_update_external(tests_run, tests_run - tests_failed);
    scoreboard_save();

    printf("[External] => %d total, %d passed.\n", tests_run, (tests_run - tests_failed));
}
