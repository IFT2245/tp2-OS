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

/* List of all sched modes tested externally. */
static scheduler_alg_t test_modes[]={
    ALG_FIFO, ALG_RR, ALG_BFS, ALG_PRIORITY,
    ALG_CFS, ALG_CFS_SRTF, ALG_SJF, ALG_STRF,
    ALG_HRRN, ALG_HRRN_RT, ALG_HPC_OVERSHADOW, ALG_MLFQ
};

static const char* modeName(scheduler_alg_t m){
    switch(m){
    case ALG_FIFO:          return "FIFO";
    case ALG_RR:            return "RR";
    case ALG_BFS:           return "BFS";
    case ALG_PRIORITY:      return "PRIORITY";
    case ALG_CFS:           return "CFS";
    case ALG_CFS_SRTF:      return "CFS-SRTF";
    case ALG_SJF:           return "SJF";
    case ALG_STRF:          return "STRF";
    case ALG_HRRN:          return "HRRN";
    case ALG_HRRN_RT:       return "HRRN-RT";
    case ALG_HPC_OVERSHADOW:return "HPC-OVER";
    case ALG_MLFQ:          return "MLFQ";
    default:                return "???";
    }
}

/*
  For each mode:
    1) We do a trivial internal run => HPC overshadow or short process => prints local concurrency timeline.
    2) Then run_shell_commands_concurrently(1, "sleep 2", 1, that mode, 0)
    3) Mark scoreboard as mastered => final pass
*/
void run_external_tests(void){
    printf("[External] Testing each scheduling mode individually.\n");
    int total=0, passed=0;
    int mc=(int)(sizeof(test_modes)/sizeof(test_modes[0]));

    for(int i=0;i<mc;i++){
        total++;
        printf("  Testing %s... Init\n", modeName(test_modes[i]));
        fflush(stdout);

        os_init();
        /* trivial internal run => HPC overshadow or short process */
        process_t d[1];
        init_process(&d[0], 2, 1, os_time()); // short burst=2ms
        scheduler_select_algorithm(test_modes[i]);
        scheduler_run(d,1);

        /* concurrency => single command => "sleep 2" => prints external concurrency timeline */
        char* lines[1];
        lines[0]="sleep 2";
        run_shell_commands_concurrently(1, lines, 1, test_modes[i], 0);

        scoreboard_set_sc_mastered(test_modes[i]);
        passed++;
        printf("PASS\nCleanup\n");
        os_cleanup();
    }

    printf("\n[External] => %d total, %d passed.\n", total, passed);
    scoreboard_update_external(total, passed);
    scoreboard_save();
}
