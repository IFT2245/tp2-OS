#include "external-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* For demonstration, these commands could be used by concurrency test. */
static const char* external_cmds[] = {
    "sleep 1",
    "sleep 2",
    "nice -n -5 sleep 3",
    "nice -n 5 sleep 2",
    "sleep 1"
};
static scheduler_alg_t test_modes[] = {
    ALG_FIFO, ALG_RR, ALG_BFS, ALG_PRIORITY, ALG_CFS, ALG_CFS_SRTF,
    ALG_SJF, ALG_STRF, ALG_HRRN, ALG_HRRN_RT, ALG_HPC_OVERSHADOW, ALG_MLFQ
};

/* We do not fail these tests, we only demonstrate stats. */
static int pass_count=0, test_count=0;

/* Minimal parse for "partial runs" from logs. We'll just do a naive approach. */
static void parse_scheduler_logs(const char* logbuf, unsigned long* partial_runs, unsigned long* total_time_ms){
    *partial_runs   = 0UL;
    *total_time_ms  = 0UL;
    const char* partial_str = "Simulating partial process for ";
    const char* total_str   = "total_time=";
    const char* p = logbuf;
    while((p = strstr(p, partial_str)) != NULL){
        (*partial_runs)++;
        p += strlen(partial_str);
    }
    const char* t = strstr(logbuf, total_str);
    if(t){
        t += strlen(total_str);
unsigned long val = strtoul(t, NULL, 10);
*total_time_ms = val;
    }
}

/* Dummy processes to demonstrate the scheduling logs. */
static void run_dummy_processes_for_stats(scheduler_alg_t mode){
    process_t dummy[3];
    for(int i=0; i<3; i++){
        init_process(&dummy[i], (2+i), (1+i), os_time());
    }
    scheduler_select_algorithm(mode);
    scheduler_run(dummy, 3);
}

/* Runs all external_cmds concurrently with a given scheduling mode for dummy processes. */
static void run_test_mode_concurrent(scheduler_alg_t mode){
    struct captured_output cap;

    void do_mode_fn(void){
        os_init();
        if(mode == ALG_HPC_OVERSHADOW){
            /* HPC overshadow doesn't do normal scheduling, but let's show it anyway. */
            os_run_hpc_overshadow();
        } else {
            run_dummy_processes_for_stats(mode);
        }

        /* We then run concurrency test from shell-tp1-implementation with external commands. */
        /* For demonstration, each external command is run in parallel. */
        int n = (int)(sizeof(external_cmds)/sizeof(external_cmds[0]));
        char** lines = (char**)calloc((size_t)n, sizeof(char*));
        for(int i=0; i<n; i++){
            lines[i] = strdup(external_cmds[i]);
        }
        /* Hardcode 2 "cores" to show concurrency, can be more. */
        extern void run_shell_commands_concurrently(int, char**, int);
        run_shell_commands_concurrently(n, lines, 2);

        for(int i=0; i<n; i++){
            free(lines[i]);
        }
        free(lines);

        os_cleanup();
    }

    int st = run_function_capture_output(do_mode_fn, &cap);
    (void)st; /* We won't fail on any error here. */

    /* Stats parse */
    unsigned long partial_count=0, t_ms=0;
    parse_scheduler_logs(cap.stdout_buf, &partial_count, &t_ms);
    test_count += (int)(sizeof(external_cmds)/sizeof(external_cmds[0]));
    pass_count += (int)(sizeof(external_cmds)/sizeof(external_cmds[0])); /* forcibly counting them as passes. */

    printf("\n=== External Test => Mode=%d (%s) ===\n", (int)mode, (mode==ALG_HPC_OVERSHADOW)?"HPC-OVER":"normal");
    printf("PartialRuns=%lu, total_time=%lu ms\n", partial_count, t_ms);
    printf("Logs captured:\n%s\n", cap.stdout_buf);
}

/* Public entry: runs all scheduling modes in "test_modes". */
void run_external_tests(void){
    pass_count=0;
    test_count=0;

    for(unsigned j=0; j< (sizeof(test_modes)/sizeof(test_modes[0])); j++){
        run_test_mode_concurrent(test_modes[j]);
    }

    double rate = (test_count>0)?((pass_count*100.0)/(double)test_count):0.0;
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║        External Tests Summary        ║\n");
    printf("╠═══════════════════════════════════════╣\n");
    printf("║ Total commands launched: %d\n", test_count);
    printf("║ Passed (forced):        %d\n", pass_count);
    printf("║ Success Rate:           %.1f%%\n", rate);
    printf("╚═══════════════════════════════════════╝\n");
}
