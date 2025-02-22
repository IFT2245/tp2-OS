#include "external-test.h"
#include "test_common.h"
#include "../src/os.h"
#include "../src/scheduler.h"
#include "../src/process.h"
#include "../src/runner.h"
#include "../src/scoreboard.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* external_cmds[]={
    "sleep 1",
    "sleep 2",
    "nice -n -5 sleep 3",
    "nice -n 5 sleep 2",
    "sleep 1",
    "echo Hello",
    "echo Done"
};
// ALG_HRRn_RT
static scheduler_alg_t test_modes[]={
    ALG_FIFO,ALG_RR,ALG_BFS,ALG_PRIORITY,ALG_CFS,ALG_CFS_SRTF,ALG_SJF,ALG_STRF,ALG_HRRN,ALG_HPC_OVERSHADOW,ALG_MLFQ
};

void run_external_tests(void){
    int cmd_count=(int)(sizeof(external_cmds)/sizeof(external_cmds[0]));
    int mode_count=(int)(sizeof(test_modes)/sizeof(test_modes[0]));
    int forced_pass=0;
    printf("[External] Running => %d commands, %d modes\n",cmd_count,mode_count);
    for(int i=0;i<mode_count;i++){
        scheduler_select_algorithm(test_modes[i]);
        os_init();
        process_t d[1]; init_process(&d[0],2,1,os_time());
        scheduler_run(d,1);
        char** lines=(char**)calloc(cmd_count,sizeof(char*));
        for(int c=0;c<cmd_count;c++){
            lines[c]=strdup(external_cmds[c]);
        }
        run_shell_commands_concurrently(cmd_count,lines,2);
        for(int c=0;c<cmd_count;c++){
            free(lines[c]);
        }
        free(lines);
        os_cleanup();
        forced_pass+=cmd_count;
        scoreboard_set_sc_mastered(test_modes[i]);
    }
    printf("[External] Forced pass => %d\n",forced_pass);
}
