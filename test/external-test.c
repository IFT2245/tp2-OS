#include "external-test.h"

#include "../src/runner.h"    // for run_shell_commands_concurrently()
#include "../src/safe_calls_library.h"
#include "../src/stats.h"
#include "../src/os.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "../src/logger.h"

/*
  concurrency_test_case1:
   - Creates 1 container with HPC disabled => ALG_FIFO
   - Creates 1 container with HPC => ALG_RR + ALG_HPC
   - Runs them => checks if all processes ended
*/
void concurrency_test_case1(void)
{
    /* Container #1 => 2 procs, no HPC, FIFO, 2 cores */
    process_t c1_procs[2];
    init_process(&c1_procs[0], 3, 5, 0);
    init_process(&c1_procs[1], 5, 7, 0);
    container_t c1;
    container_init(&c1, 2, false, ALG_FIFO, ALG_NONE,
                   c1_procs, 2,
                   NULL,     0);

    /* Container #2 => HPC => main=ALG_RR, HPC=ALG_HPC, 2 cores */
    process_t c2_main[2];
    init_process(&c2_main[0], 2, 2, 0);
    init_process(&c2_main[1], 6, 4, 0);

    process_t c2_hpc[1];
    init_process(&c2_hpc[0], 4, 1, 0);

    container_t c2;
    container_init(&c2, 2, true, ALG_RR, ALG_HPC,
                   c2_main, 2,
                   c2_hpc,  1);

    container_t containers[2] = { c1, c2 };
    orchestrator_run(containers, 2);

    /* Check quickly if everything ended. (All processes should have remaining_time=0). */
    bool pass = true;
    for(int i=0; i<2; i++){
        if(c1_procs[i].remaining_time>0){
            pass = false;
        }
    }
    for(int i=0; i<2; i++){
        if(c2_main[i].remaining_time>0){
            pass = false;
        }
    }
    if(c2_hpc[0].remaining_time>0){
        pass=false;
    }

    if(pass){
        printf("[TestCase1] => PASS\n");
        stats_inc_tests_passed(1);
        scoreboard_update_external(1,1); /* or scoreboard_update_modes(1,1), up to you */
    } else {
        printf("[TestCase1] => FAIL => Some process did not finish.\n");
        stats_inc_tests_failed(1);
        scoreboard_update_external(1,0);
    }
}

/*
  concurrency_test_case2:
   - 2 containers each with HPC, BFS + HPC, Priority + HPC
   - 3 processes in each container
*/
void concurrency_test_case2(void)
{
    process_t c1_main[2];
    init_process(&c1_main[0], 4, 3, 0);
    init_process(&c1_main[1], 4, 2, 0);

    process_t c1_hpc[1];
    init_process(&c1_hpc[0], 5, 1, 0);

    container_t c1;
    container_init(&c1, 3, true, ALG_BFS, ALG_HPC,
                   c1_main, 2,
                   c1_hpc,  1);

    /* second container => Priority + HPC, 2 cores */
    process_t c2_main[2];
    init_process(&c2_main[0], 3, 1, 0);
    init_process(&c2_main[1], 2, 5, 0);

    process_t c2_hpc[1];
    init_process(&c2_hpc[0], 6, 2, 0);

    container_t c2;
    container_init(&c2, 2, true, ALG_PRIORITY, ALG_HPC,
                   c2_main, 2,
                   c2_hpc,  1);

    container_t containers[2] = { c1, c2 };
    orchestrator_run(containers, 2);

    /* check pass/fail: did all processes finish? */
    bool pass = true;
    for(int i=0; i<2; i++){
        if(c1_main[i].remaining_time>0) pass=false;
    }
    if(c1_hpc[0].remaining_time>0) pass=false;

    for(int i=0; i<2; i++){
        if(c2_main[i].remaining_time>0) pass=false;
    }
    if(c2_hpc[0].remaining_time>0) pass=false;

    if(pass){
        printf("[TestCase2] => PASS\n");
        stats_inc_tests_passed(1);
        scoreboard_update_external(1,1); /* or scoreboard_update_modes(1,1), etc. */
    } else {
        printf("[TestCase2] => FAIL => Some process did not finish.\n");
        stats_inc_tests_failed(1);
        scoreboard_update_external(1,0);
    }
}

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>XXZ

/*
 * concurrency_test_shell():
 *  - We run multiple "shell-tp1-implementation" in parallel
 *    passing them "sleep X" commands.
 *  - We do ephemeral container usage automatically in container_run,
 *    so let's just fork shells here.
 *
 *  - We'll color the logs, measure pass/fail if all shells completed
 *    within some timeframe.
 */
void concurrency_test_shell(void)
{
    log_info(CLR_BOLD CLR_YELLOW "=== Starting external shell concurrency test ===" CLR_RESET);

    /* We'll spawn e.g. 3 shells in parallel, each "sleep N". */
    int N=3;
    pid_t pids[3];
    for(int i=0;i<N;i++){
        int pipefd[2];
        pipe(pipefd);

        pid_t c=fork();
        if(c<0){
            log_error("fork failed!");
            continue;
        }
        else if(c==0){
            close(pipefd[1]);
            /* child => read from pipefd[0] => STDIN => execl shell */
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            execl("./shell-tp1-implementation","shell-tp1-implementation",(char*)NULL);
            _exit(127);
        }
        else {
            pids[i]=c;
            close(pipefd[0]);
            /* pass "sleep i+1" plus "exit" to shell's stdin */
            char cmd[64];
            snprintf(cmd,sizeof(cmd),"sleep %d\nexit\n", i+1);
            write(pipefd[1], cmd, strlen(cmd));
            close(pipefd[1]);
        }
    }

    /* Wait for them all. */
    bool pass=true;
    for(int i=0;i<N;i++){
        int status=0;
        waitpid(pids[i], &status, 0);
        if(!WIFEXITED(status)){
            pass=false;
        }
    }

    if(pass){
        log_info(CLR_BOLD CLR_GREEN "[Shell concurrency test] => PASS" CLR_RESET);
        scoreboard_update_edge(1,1);
        stats_inc_tests_passed(1);
    } else {
        log_info(CLR_BOLD CLR_RED "[Shell concurrency test] => FAIL" CLR_RESET);
        scoreboard_update_edge(1,0);
        stats_inc_tests_failed(1);
    }
}

static void concurrency_test_basic(void){
  process_t p[2];init_process(&p[0],3,5,0);init_process(&p[1],5,7,2);container_t c;container_init(&c,2,false,ALG_FIFO,ALG_NONE,p,2,NULL,0,50);container_t arr[1]={c};orchestrator_run(arr,1);
  bool pass=true;if(p[0].remaining_time>0)pass=false;if(p[1].remaining_time>0)pass=false;
  if(pass){printf(CLR_BOLD CLR_GREEN"Basic PASS\n"CLR_RESET);scoreboard_update_basic(1,1);stats_inc_tests_passed(1);}else{printf(CLR_BOLD CLR_RED"Basic FAIL\n"CLR_RESET);scoreboard_update_basic(1,0);stats_inc_tests_failed(1);}
}
static void concurrency_test_normal(void){
  process_t p[2];init_process(&p[0],4,3,0);init_process(&p[1],2,2,1);container_t c;container_init(&c,2,false,ALG_RR,ALG_NONE,p,2,NULL,0,50);container_t arr[1]={c};orchestrator_run(arr,1);
  bool pass=true;if(p[0].remaining_time>0)pass=false;if(p[1].remaining_time>0)pass=false;
  if(pass){printf(CLR_BOLD CLR_GREEN"Normal PASS\n"CLR_RESET);scoreboard_update_normal(1,1);stats_inc_tests_passed(1);}else{printf(CLR_BOLD CLR_RED"Normal FAIL\n"CLR_RESET);scoreboard_update_normal(1,0);stats_inc_tests_failed(1);}
}
static void concurrency_test_edge_shell(void){
  int N=3;pid_t pids[3];bool pass=true;for(int i=0;i<N;i++){int pipefd[2];pipe(pipefd);pid_t c=fork();if(c<0){pass=false;close(pipefd[0]);close(pipefd[1]);continue;}else if(c==0){close(pipefd[1]);dup2(pipefd[0],STDIN_FILENO);close(pipefd[0]);execl("./shell-tp1-implementation","shell-tp1-implementation",(char*)NULL);_exit(127);}else{pids[i]=c;close(pipefd[0]);char cmd[64];snprintf(cmd,sizeof(cmd),"sleep %d\nexit\n",i+1);write(pipefd[1],cmd,strlen(cmd));close(pipefd[1]);}}
  for(int i=0;i<N;i++){int st=0;waitpid(pids[i],&st,0);if(!WIFEXITED(st))pass=false;}
  if(pass){printf(CLR_BOLD CLR_GREEN"Edge Shell PASS\n"CLR_RESET);scoreboard_update_edge(1,1);stats_inc_tests_passed(1);}else{printf(CLR_BOLD CLR_RED"Edge Shell FAIL\n"CLR_RESET);scoreboard_update_edge(1,0);stats_inc_tests_failed(1);}
}
static void concurrency_test_hidden(void){
  process_t p[1];init_process(&p[0],8,1,0);container_t c;container_init(&c,2,true,ALG_BFS,ALG_HPC,p,1,NULL,0,6);container_t arr[1]={c};orchestrator_run(arr,1);
  bool pass=(p[0].remaining_time==0);if(pass){printf(CLR_BOLD CLR_GREEN"Hidden HPC PASS\n"CLR_RESET);scoreboard_update_hidden(1,1);stats_inc_tests_passed(1);}else{printf(CLR_BOLD CLR_RED"Hidden HPC FAIL\n"CLR_RESET);scoreboard_update_hidden(1,0);stats_inc_tests_failed(1);}
}

