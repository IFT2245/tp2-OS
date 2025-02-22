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
    if(!unlocked_basic){printf("BASIC locked.\n");return;}
    {
        int t=0,p=0; run_basic_tests(&t,&p);
        scoreboard_update_basic(t,p);
    }
    if(!unlocked_normal){printf("NORMAL locked.\n");return;}
    {
        int t=0,p=0; run_normal_tests(&t,&p);
        scoreboard_update_normal(t,p);
    }
    if(!unlocked_modes){printf("MODES locked.\n");return;}
    {
        int t=0,p=0; run_modes_tests(&t,&p);
        scoreboard_update_modes(t,p);
    }
    if(!unlocked_edge){printf("EDGE locked.\n");return;}
    {
        int t=0,p=0; run_edge_tests(&t,&p);
        scoreboard_update_edge(t,p);
    }
    if(!unlocked_hidden){printf("HIDDEN locked.\n");return;}
    {
        int t=0,p=0; run_hidden_tests(&t,&p);
        scoreboard_update_hidden(t,p);
    }
}

void run_external_tests_menu(void){
    extern int unlocked_external; if(!unlocked_external)return;
    run_external_tests();
}

void run_shell_commands_concurrently(int count,char** lines,int coreCount){
    if(count<=0||!lines)return;
    typedef struct {
        int p_out[2],p_err[2],p_in[2];
        pid_t pid;
        char* cmd;
        unsigned long long start_ms,end_ms;
        int core;
    } child_t;
    child_t* ch=(child_t*)calloc(count,sizeof(child_t));
    if(!ch)return;
    if(access("./shell-tp1-implementation",X_OK)!=0){
        printf("No shell-tp1-implementation.\n");
        free(ch); return;
    }
    unsigned long long start=os_time();
    int next_core=0;
    for(int i=0;i<count;i++){
        if(!lines[i]||!*lines[i]) continue;
        pipe(ch[i].p_out); pipe(ch[i].p_err); pipe(ch[i].p_in);
        pid_t c=fork();
        if(c<0){
            fprintf(stderr,"fork fail\n");
        } else if(c==0){
            close(ch[i].p_out[0]); close(ch[i].p_err[0]); close(ch[i].p_in[0]);
            dup2(ch[i].p_out[1],STDOUT_FILENO);
            dup2(ch[i].p_err[1],STDERR_FILENO);
            dup2(ch[i].p_in[1],STDIN_FILENO);
            close(ch[i].p_out[1]); close(ch[i].p_err[1]); close(ch[i].p_in[1]);
            execl("./shell-tp1-implementation","shell-tp1-implementation",(char*)NULL);
            _exit(127);
        } else {
            ch[i].pid=c; ch[i].cmd=lines[i];
            ch[i].start_ms=os_time(); ch[i].core=next_core;
            close(ch[i].p_out[1]); close(ch[i].p_err[1]); close(ch[i].p_in[0]);
            dprintf(ch[i].p_in[1],"%s\nexit\n",lines[i]);
            close(ch[i].p_in[1]);
            next_core=(next_core+1)%coreCount;
        }
    }
    for(int i=0;i<count;i++){
        if(!ch[i].pid) continue;
        waitpid(ch[i].pid,NULL,0);
        ch[i].end_ms=os_time();
    }
    unsigned long long end=os_time();
    for(int i=0;i<count;i++){
        if(!ch[i].pid) continue;
        char outb[512]={0},errb[512]={0};
        read(ch[i].p_out[0],outb,sizeof(outb)-1);
        read(ch[i].p_err[0],errb,sizeof(errb)-1);
        close(ch[i].p_out[0]); close(ch[i].p_err[0]);
        unsigned long long dur=ch[i].end_ms-ch[i].start_ms; if(!dur) dur=1;
        printf("[CORE%d] pid=%d dur=%llums cmd='%s'\n",ch[i].core,(int)ch[i].pid,dur,ch[i].cmd);
    }
    printf("Total concurrency=%llums\n",end-start);
    free(ch);
}
