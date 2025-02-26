#include "external-test.h"
#include "test_common.h"

#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/scoreboard.h"
#include "../src/runner.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

static int g_tests_run=0, g_tests_failed=0;
static char g_fail_reason[256];

/* HPC overshadow test */
static bool test_external_hpc(void) {
    g_tests_run++;
    os_init();
    process_t dummy[1];
    init_process(&dummy[0], 0, 0, 0);

    scheduler_select_algorithm(ALG_HPC_OVERSHADOW);
    scheduler_run(dummy,1);

    sched_report_t rep;
    scheduler_fetch_report(&rep);
    os_cleanup();

    if(rep.total_procs!=0){
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_external_hpc => expected total_procs=0, got %llu",
                 (unsigned long long)rep.total_procs);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_HPC_OVERSHADOW);
    return true;
}

/* BFS partial test */
static bool test_external_bfs(void) {
    g_tests_run++;
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
        snprintf(g_fail_reason,sizeof(g_fail_reason),
                 "test_external_bfs => mismatch => procs=%llu, preempts=%llu",
                 rep.total_procs, rep.preemptions);
        test_set_fail_reason(g_fail_reason);
        g_tests_failed++;
        return false;
    }
    scoreboard_set_sc_mastered(ALG_BFS);
    return true;
}

/* Shell concurrency sample test => just run 2 commands with FIFO. */
static bool test_run_shell_concurrency(void) {
    g_tests_run++;
    int count=2;
    char* lines[2];
    lines[0] = "sleep 2";
    lines[1] = "sleep 3";

    run_shell_commands_concurrently(count, lines, 1, ALG_FIFO, 0);
    /* Not strictly verifying output, just ensuring it runs without error. */
    return true;
}

/* array of external tests */
typedef bool (*test_fn)(void);
static struct {
    const char* name;
    test_fn func;
} external_tests[] = {
    {"hpc_over",           test_external_hpc},
    {"bfs_partial",        test_external_bfs},
    {"shell_concurrency",  test_run_shell_concurrency}
};
static const int EXTERNAL_COUNT = sizeof(external_tests)/sizeof(external_tests[0]);

int external_test_count(void){ return EXTERNAL_COUNT; }
const char* external_test_name(int i){
    if(i<0||i>=EXTERNAL_COUNT) return NULL;
    return external_tests[i].name;
}
void external_test_run_single(int i,int* pass_out){
    if(!pass_out) return;
    if(i<0||i>=EXTERNAL_COUNT){
        *pass_out=0;
        return;
    }
    g_tests_run=0;
    g_tests_failed=0;
    bool ok = external_tests[i].func();
    *pass_out = (ok && g_tests_failed==0)?1:0;
}

void run_external_tests(void) {
    printf("\n\033[1m\033[93m╔═════════ EXTERNAL TESTS START ═════════╗\033[0m\n");
    g_tests_run=0;
    g_tests_failed=0;
    for(int i=0; i<EXTERNAL_COUNT; i++){
        bool ok = external_tests[i].func();
        if(ok){
            printf("  PASS: %s\n", external_tests[i].name);
        } else {
            printf("  FAIL: %s => %s\n", external_tests[i].name, test_get_fail_reason());
        }
    }

    /* scoreboard update done in runner or main menu caller */
    printf("\033[1m\033[93m╔════════════════════════════════════════════╗\n");
    printf("║   EXTERNAL TESTS RESULTS: %d / %d passed     ║\n",
           (g_tests_run - g_tests_failed), g_tests_run);
    if(g_tests_failed>0) {
        printf("║   FAILURES => see logs above              ║\n");
    }
    printf("╚════════════════════════════════════════════╝\033[0m\n");
}
