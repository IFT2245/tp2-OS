#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "runner.h"
#include "scoreboard.h"
#include "os.h"
#include "scheduler.h"
#include "worker.h"
#include "../test/basic-test.h"
#include "../test/normal-test.h"
#include "../test/modes-test.h"
#include "../test/edge-test.h"
#include "../test/hidden-test.h"
#include "../test/external-test.h"

/* run_all_levels() => run each test suite if unlocked. */
void run_all_levels(void){
    extern int unlocked_basic, unlocked_normal, unlocked_modes,
                unlocked_edge, unlocked_hidden;
    if(!unlocked_basic){
        printf("BASIC locked.\n");
        return;
    }
    {
        int t=0,p=0; run_basic_tests(&t,&p);
        scoreboard_update_basic(t,p);
        scoreboard_save();
    }

    if(!unlocked_normal){
        printf("NORMAL locked.\n");
        return;
    }
    {
        int t=0,p=0; run_normal_tests(&t,&p);
        scoreboard_update_normal(t,p);
        scoreboard_save();
    }

    if(!unlocked_modes){
        printf("MODES locked.\n");
        return;
    }
    {
        int t=0,p=0; run_modes_tests(&t,&p);
        scoreboard_update_modes(t,p);
        scoreboard_save();
    }

    if(!unlocked_edge){
        printf("EDGE locked.\n");
        return;
    }
    {
        int t=0,p=0; run_edge_tests(&t,&p);
        scoreboard_update_edge(t,p);
        scoreboard_save();
    }

    if(!unlocked_hidden){
        printf("HIDDEN locked.\n");
        return;
    }
    {
        int t=0,p=0; run_hidden_tests(&t,&p);
        scoreboard_update_hidden(t,p);
        scoreboard_save();
    }
}

/* External tests menu => calls run_external_tests() from external-test.c. */
void run_external_tests_menu(void){
    extern int unlocked_external;
    if(!unlocked_external) return;
    run_external_tests();
}

/*
  For concurrency: single-line concurrency timeline per core,
  minimal duplication, no extra "Cleanup" lines. HPC overshadow or local
  concurrency are separate from here. This function prints an
  ASCII bar for each core, plus a short line for each process.
*/
static int cmp_events(const void* a,const void* b){
    uint64_t x=((uint64_t*)a)[0];
    uint64_t y=((uint64_t*)b)[0];
    if(x<y)return -1; if(x>y)return 1; return 0;
    return 0;
}

void run_shell_commands_concurrently(int count,char** lines,int coreCount,
                                     int mode,int allModes)
{
    if(count<=0||!lines) return;

    typedef struct {
        pid_t pid;
        char* cmd;
        int   core;
        uint64_t start_ms;
        uint64_t end_ms;
        int   p_out[2];
        int   p_err[2];
        int   p_in[2];
    } child_t;

    scheduler_alg_t modes_arr[]={
        ALG_FIFO, ALG_RR, ALG_BFS, ALG_PRIORITY,
        ALG_CFS, ALG_CFS_SRTF, ALG_SJF, ALG_STRF,
        ALG_HRRN, ALG_HRRN_RT, ALG_HPC_OVERSHADOW, ALG_MLFQ
    };
    const char* modeNames[]={
        "FIFO","RR","BFS","PRIORITY",
        "CFS","CFS-SRTF","SJF","STRF",
        "HRRN","HRRN-RT","HPC-OVER","MLFQ"
    };
    int mode_count=(int)(sizeof(modes_arr)/sizeof(modes_arr[0]));

    int from=0,to=mode_count-1;
    if(!allModes){
        if(mode<0||mode>=mode_count){
            printf("Invalid scheduling mode.\n");
            return;
        }
        from=to=mode;
    }

    for(int m=from;m<=to;m++){
        printf("\n\033[95m=== Concurrency with %s ===\033[0m\n", modeNames[m]);

        child_t* ch=(child_t*)calloc(count,sizeof(child_t));
        if(!ch) return;

        if(access("./shell-tp1-implementation",X_OK)!=0){
            printf("No shell-tp1-implementation found.\n");
            free(ch);
            return;
        }
        uint64_t global_start=os_time();
        int next_core=0;

        /* Fork each child */
        for(int i=0;i<count;i++){
            if(!lines[i]||!*lines[i]) continue;
            pipe(ch[i].p_out);
            pipe(ch[i].p_err);
            pipe(ch[i].p_in);

            pid_t c=fork();
            if(c<0){
                fprintf(stderr,"fork() error\n");
            } else if(c==0){
                /* child */
                close(ch[i].p_out[0]);
                close(ch[i].p_err[0]);
                close(ch[i].p_in[0]);
                dup2(ch[i].p_out[1],STDOUT_FILENO);
                dup2(ch[i].p_err[1],STDERR_FILENO);
                dup2(ch[i].p_in[1],STDIN_FILENO);
                close(ch[i].p_out[1]);
                close(ch[i].p_err[1]);
                close(ch[i].p_in[1]);

                execl("./shell-tp1-implementation","shell-tp1-implementation",(char*)NULL);
                _exit(127);
            } else {
                /* parent */
                ch[i].pid=c;
                ch[i].cmd=lines[i];
                ch[i].core=next_core;
                ch[i].start_ms=os_time();

                close(ch[i].p_out[1]);
                close(ch[i].p_err[1]);
                close(ch[i].p_in[0]);
                /* Write command + exit */
                dprintf(ch[i].p_in[1],"%s\nexit\n",ch[i].cmd);
                close(ch[i].p_in[1]);
                next_core=(next_core+1)%coreCount;
            }
        }

        /* Wait for them */
        for(int i=0;i<count;i++){
            if(!ch[i].pid) continue;
            waitpid(ch[i].pid,NULL,0);
            ch[i].end_ms=os_time();
        }
        uint64_t global_end=os_time();

        /* Drain leftover. */
        for(int i=0;i<count;i++){
            if(!ch[i].pid) continue;
            char outb[256]={0},errb[256]={0};
            read(ch[i].p_out[0], outb,sizeof(outb)-1);
            read(ch[i].p_err[0], errb,sizeof(errb)-1);
            close(ch[i].p_out[0]);
            close(ch[i].p_err[0]);
        }

        /* Single-line concurrency timeline => one line per core. */
        printf("\n\033[93m[Concurrency Timeline => %s]\033[0m\n", modeNames[m]);

        /* min/max times for child runs */
        uint64_t min_start=(uint64_t)-1, max_end=0ULL;
        for(int i=0;i<count;i++){
            if(ch[i].start_ms<min_start) min_start=ch[i].start_ms;
            if(ch[i].end_ms>max_end)     max_end=ch[i].end_ms;
        }
        if(min_start==(uint64_t)-1) min_start=0;
        if(max_end<min_start) max_end=min_start;

        /* concurrency stats */
        uint64_t(*events)[2] = calloc(count*2,sizeof(uint64_t[2]));
        int eidx=0;
        for(int i=0;i<count;i++){
            events[eidx][0]=ch[i].start_ms; events[eidx][1]=1; eidx++;
            events[eidx][0]=ch[i].end_ms;   events[eidx][1]=(uint64_t)-1; eidx++;
        }
        qsort(events,eidx,sizeof(events[0]),cmp_events);
        int concurrency=0,peak=0;
        double sum=0.0;
        uint64_t last_t=min_start;
        for(int i=0;i<eidx;i++){
            uint64_t t=events[i][0];
            if(t<last_t) t=last_t;
            uint64_t dt=(t>last_t?(t-last_t):0);
            sum+=(concurrency*dt);
            if(events[i][1]==(uint64_t)-1) concurrency--;
            else concurrency++;
            if(concurrency>peak) peak=concurrency;
            last_t=t;
        }
        free(events);
        uint64_t length=(max_end>min_start?(max_end-min_start):1ULL);
        double avg=(length>0)?(sum/(double)length):0.0;

        /* Build single-line ASCII bars per core. */
        const int COLS=30;
        uint64_t span=(max_end>min_start?(max_end-min_start):1ULL);

        for(int c=0;c<coreCount;c++){
            char row[COLS+1];
            memset(row,' ',sizeof(row));
            row[COLS]='\0';

            /* Mark intervals from each process if core matches c. */
            for(int i=0;i<count;i++){
                if(ch[i].core==c){
                    uint64_t st=ch[i].start_ms;
                    uint64_t en=ch[i].end_ms;
                    for(int col=0;col<COLS;col++){
                        uint64_t t=min_start+(col*span/COLS);
                        if(t>=st && t<en){
                            row[col]='█';
                        }
                    }
                }
            }
            printf("core%d: [%s]\n",c,row);
            /* Then print each process line for clarity. */
            for(int i=0;i<count;i++){
                if(ch[i].core==c){
                    uint64_t dur=(ch[i].end_ms>ch[i].start_ms?
                                  ch[i].end_ms - ch[i].start_ms:0ULL);
                    printf("  P%d => start=%llu end=%llu dur=%llums cmd=\"%s\"\n",
                           i+1,
                           (unsigned long long)ch[i].start_ms,
                           (unsigned long long)ch[i].end_ms,
                           (unsigned long long)dur,
                           ch[i].cmd?ch[i].cmd:"");
                }
            }
            printf("\n");
        }

        printf("\033[96mPeak concurrency=%d, Average concurrency=%.2f\033[0m\n",peak,avg);
        uint64_t total_concurrency=(global_end>global_start?
                                    global_end-global_start:0ULL);
        printf("\033[92mTotal concurrency time=%llums\033[0m\n\n",
               (unsigned long long)total_concurrency);

        free(ch);
    }
}
