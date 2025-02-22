#include "runner.h"
#include "scoreboard.h"
#include "os.h"
#include "../test/basic-test.h"
#include "../test/normal-test.h"
#include "../test/modes-test.h"
#include "../test/edge-test.h"
#include "../test/hidden-test.h"
#include "../test/external-test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void run_all_levels(void){
    extern int unlocked_basic,unlocked_normal,unlocked_modes,unlocked_edge,unlocked_hidden;
    if(!unlocked_basic){
        printf("[Runner] BASIC locked.\n");
        return;
    }
    printf("== BASIC ==\n");
    {
        int t=0,p=0; run_basic_tests(&t,&p);
        scoreboard_update_basic(t,p);
    }
    if(!unlocked_normal){
        printf("[Runner] NORMAL locked.\n");
        return;
    }
    printf("== NORMAL ==\n");
    {
        int t=0,p=0; run_normal_tests(&t,&p);
        scoreboard_update_normal(t,p);
    }
    if(!unlocked_modes){
        printf("[Runner] MODES locked.\n");
        return;
    }
    printf("== MODES ==\n");
    {
        int t=0,p=0; run_modes_tests(&t,&p);
        scoreboard_update_modes(t,p);
    }
    if(!unlocked_edge){
        printf("[Runner] EDGE locked.\n");
        return;
    }
    printf("== EDGE ==\n");
    {
        int t=0,p=0; run_edge_tests(&t,&p);
        scoreboard_update_edge(t,p);
    }
    if(!unlocked_hidden){
        printf("[Runner] HIDDEN locked.\n");
        return;
    }
    printf("== HIDDEN ==\n");
    {
        int t=0,p=0; run_hidden_tests(&t,&p);
        scoreboard_update_hidden(t,p);
    }
    printf("[Runner] All possible levels executed.\n");
}

void run_external_tests_menu(void){
    extern int unlocked_external;
    if(!unlocked_external){
        printf("[Runner] External tests locked.\n");
        return;
    }
    run_external_tests();
}

void run_shell_commands_concurrently(int count, char** lines, int coreCount){
    if(count<=0 || !lines) return;
    typedef struct {
        int p_out[2], p_err[2], p_in[2];
        pid_t pid;
        char* cmd;
        unsigned long long start_ms, end_ms;
        int core_assigned;
    } child_info_t;

    child_info_t* children= (child_info_t*)calloc((size_t)count, sizeof(child_info_t));
    if(!children) return;

    if(access("./shell-tp1-implementation", X_OK)!=0){
        printf("[Runner] shell-tp1-implementation not found.\n");
        free(children);
        return;
    }
    unsigned long long g_start = os_time();
    printf("[Runner] Concurrency => %d processes, %d cores\n", count, coreCount);

    int next_core=0;
    for(int i=0; i<count; i++){
        if(!lines[i] || !*lines[i]){
            printf("[Runner] Empty command for #%d\n", i+1);
            continue;
        }
        pipe(children[i].p_out);
        pipe(children[i].p_err);
        pipe(children[i].p_in);

        pid_t c = fork();
        if(c<0){
            fprintf(stderr,"[Runner] fork failed for child #%d\n", i+1);
        } else if(c==0){
            close(children[i].p_out[0]);
            close(children[i].p_err[0]);
            close(children[i].p_in[0]);
            dup2(children[i].p_out[1], STDOUT_FILENO);
            dup2(children[i].p_err[1], STDERR_FILENO);
            dup2(children[i].p_in[1],  STDIN_FILENO);
            close(children[i].p_out[1]);
            close(children[i].p_err[1]);
            close(children[i].p_in[1]);
            execl("./shell-tp1-implementation","shell-tp1-implementation",(char*)NULL);
            _exit(127);
        } else {
            children[i].pid = c;
            children[i].cmd = lines[i];
            children[i].start_ms = os_time();
            children[i].core_assigned = next_core;
            close(children[i].p_out[1]);
            close(children[i].p_err[1]);
            close(children[i].p_in[0]);
            dprintf(children[i].p_in[1], "%s\nexit\n", lines[i]);
            close(children[i].p_in[1]);
            next_core = (next_core+1) % coreCount;
        }
    }

    for(int i=0; i<count; i++){
        if(!children[i].cmd || !children[i].pid) continue;
        int st=0;
        waitpid(children[i].pid, &st, 0);
        children[i].end_ms = os_time();
    }

    unsigned long long g_end  = os_time();
    unsigned long long total_time = g_end - g_start;
    if(total_time<1) total_time=1;

    for(int i=0; i<count; i++){
        if(!children[i].cmd || !children[i].pid) continue;
        char buf_out[512]={0}, buf_err[512]={0};
        read(children[i].p_out[0], buf_out, sizeof(buf_out)-1);
        read(children[i].p_err[0], buf_err, sizeof(buf_err)-1);
        close(children[i].p_out[0]);
        close(children[i].p_err[0]);

        unsigned long long took = children[i].end_ms - children[i].start_ms;
        if(took<1) took=1;
        unsigned long long bar_len = took/50ULL;
        if(bar_len<1) bar_len=1;
        if(bar_len>100) bar_len=100;

        static const char* colors[]={
            "\033[91m","\033[92m","\033[93m","\033[94m",
            "\033[95m","\033[96m","\033[97m","\033[90m"
        };
        const char* color = colors[ children[i].core_assigned % 8 ];
        char timeline[128];
        memset(timeline, '=', bar_len);
        timeline[bar_len] = '\0';

        printf("[C%d] PID=%d | Took=%llums | %s%s\033[0m | CMD='%s'\n",
               children[i].core_assigned, (int)children[i].pid,
               took, color, timeline, children[i].cmd);
    }
    printf("[Runner] Total concurrency time=%llums\n", total_time);
    free(children);
}
