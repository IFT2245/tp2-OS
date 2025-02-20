/*
 * scheduler.c
 *
 * A final "maximal" version focusing on:
 *   - 5 test suites in the order: Basic(0), Normal(1), Modes(2), Edge(3), Hidden(4).
 *   - Each suite requires >=60% success to unlock the next, up to level=5 => all unlocked.
 *   - Enhanced user experience with:
 *       1) ASCII banners
 *       2) Scoreboard to see partial success rates
 *       3) Additional disclaimers about HPC overshadow or container ephemeral usage
 *       4) A "GAME OVER" message with final aggregated score out of 500
 *   - UI options for:
 *       [1] Next test in progression
 *       [2] OS-based concurrency UI
 *       [3] ReadyQueue concurrency UI
 *       [4] Single Worker concurrency (the worker demo)
 *       [5] Show partial scoreboard
 *       [6] Re-run a previous test suite (for improvement attempts)
 *   - If compiled with -DUSE_OPENGL_UI, we also show an option [7] to run direct OpenGL-based OS UI
 *
 * This file depends on aggregator references for the five test suites:
 *    run_basic_tests(), run_normal_tests(), run_modes_tests(), run_edge_tests(), run_hidden_tests().
 *
 * Each aggregator prints out a "Success rate: X%" or returns 0 => 100%, 1 => 0% if fallback.
 * We parse those logs from a pipe, store them in g_suite_scores[5], each in [0..100].
 * If a suite's score >= 60 => we unlock the next level. If we reach level=5 => final "GAME OVER".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "os.h"
#include "ready.h"
#include "worker.h"
#include "safe_calls_library.h"
#include "../test/basic-test.h"
#include "../test/normal-test.h"
#include "../test/modes-test.h"
#include "../test/edge-test.h"
#include "../test/hidden-test.h"

/* 0 => basic, 1 => normal, 2 => modes, 3 => edge, 4 => hidden, 5 => done */
static int g_current_level = 0;
static double g_suite_scores[5] = {0,0,0,0,0};

/* Each aggregator in the new order: */
static int aggregator_basic(void){return run_basic_tests();}
static int aggregator_normal(void){return run_normal_tests();}
static int aggregator_modes(void){return run_modes_tests();}
static int aggregator_edge(void){return run_edge_tests();}
static int aggregator_hidden(void){return run_hidden_tests();}

/* For printing user-friendly test names in UI: */
static const char* g_suite_names[5] = {
    "BASIC", "NORMAL", "MODES", "EDGE", "HIDDEN"
};

/* The aggregator function pointers in the chosen order: */
static int (*g_suites[5])(void) = {
    aggregator_basic,
    aggregator_normal,
    aggregator_modes,
    aggregator_edge,
    aggregator_hidden
};

/*
 * We'll do a pipe-based approach to parse aggregator logs.
 * If aggregator prints "Success rate: X%", we capture that.
 * Otherwise fallback => aggregator returns 0 => 100%, else 0%.
 */
static double run_suite_and_parse(int index) {
    if(index<0 || index>4) return 0.0;
    int fd[2];
    if(pipe(fd)<0) return 0.0;

    fflush(stdout);
    int old_stdout = dup(STDOUT_FILENO);
    dup2(fd[1], STDOUT_FILENO);
    close(fd[1]);

    /* aggregator returns 0 or 1. We'll do aggregator now. */
    int rc = g_suites[index]();

    fflush(stdout);
    dup2(old_stdout, STDOUT_FILENO);
    close(old_stdout);

    char buffer[2048];
    ssize_t n = read(fd[0], buffer, sizeof(buffer)-1);
    close(fd[0]);
    if(n<0) n=0;
    buffer[n]='\0';

    /* parse logs for "Success rate: X%". fallback => if rc==0 => 100, else 0. */
    double rate;
    const char* p=strstr(buffer,"Success rate:");
    if(!p) {
        p=strstr(buffer,"success rate:");
    }
    if(p) {
        p += 13; /* skip "Success rate:" or "success rate:" */
        rate=parse_float_strtof(p, 500);
        if(rate<0.0) rate=0.0;
        if(rate>100.0) rate=100.0;
    } else {
        if(rc==0) rate=100.0;
        else rate=0.0;
    }
    return rate;
}

/*
 * If we pass => g_current_level++.
 * If g_current_level=5 => all done => show final "GAME OVER".
 */
static void do_next_test(void) {
    if(g_current_level>=5) {
        printf("[Scheduler] All 5 test suites are already done. Final.\n");
        return;
    }
    int idx = g_current_level;
    printf("\n===== Running %s Test Suite =====\n", g_suite_names[idx]);
    double r = run_suite_and_parse(idx);
    g_suite_scores[idx] = r;
    printf("[Scheduler] => %s suite success: %.1f%%\n", g_suite_names[idx], r);
    if(r>=60.0) {
        g_current_level++;
        if(g_current_level>=5) {
            printf("[Scheduler] All test suites completed => final 'GAME OVER'.\n");
            /* show final scoreboard. Then show game over. */
            /* or skip scoreboard here => up to you. We'll do it below: */
            int total=0;
            for(int i=0;i<5;i++){
                total += (int)g_suite_scores[i];
            }
            printf("\n==============================================\n"
                   " G A M E   O V E R !!!\n"
                   " Great job finishing all tests.\n"
                   " Final total synergy score: %d / 500\n"
                   "==============================================\n\n", total);
        } else {
            printf("[Scheduler] => next suite unlocked => level now %d/5.\n",g_current_level);
        }
    } else {
        printf("[Scheduler] => insufficient success => stuck at level %d.\n", g_current_level);
    }
}

/* Let user re-run a previously unlocked test suite (maybe to improve score). */
static void do_rerun_test(void) {
    printf("Which test suite do you want to re-run? (0=basic..4=hidden)? ");
    fflush(stdout);
    char line[32];
    if(!fgets(line,sizeof(line),stdin)) {
        return;
    }
    int i = parse_int_strtol(line,-1);
    if(i<0 || i>4) {
        printf("Invalid suite index.\n");
        return;
    }
    /* ensure it's unlocked => i < g_current_level or i < 5 if we want to allow re-run of all.
       We'll allow re-run if i<g_current_level => so if g_current_level=2 => we can re-run basic(0) or normal(1) but not modes(2) yet.
       If g_current_level=5 => all are unlocked, so i can be 0..4. */
    if(g_current_level<5 && i>=g_current_level) {
        printf("That suite is not unlocked yet (need to pass prior levels). Stuck.\n");
        return;
    }
    printf("\n===== Re-running %s test suite for potential improvement. =====\n", g_suite_names[i]);
    double old_score = g_suite_scores[i];
    double new_score = run_suite_and_parse(i);
    g_suite_scores[i] = (new_score>old_score) ? new_score : old_score; /* we keep the best. */
    printf("[Re-run] Old score=%.1f%%, New attempt=%.1f%% => final=%.1f%%\n",
           old_score, new_score, g_suite_scores[i]);
    /* If we improved a test that was the gating cause? We won't re-sequence progression automatically.
       That would require re-checking the entire chain. We'll keep it simpler:
       user can re-run but progression won't automatically skip. They can do next suite if they'd previously unlocked it.
     */
}

/* Show scoreboard for partial success. */
static void do_show_scoreboard(void){
    printf("\n===== Partial Scoreboard =====\n");
    for(int i=0;i<5;i++){
        printf("  %d) %s => %.1f%% %s\n",
               i,g_suite_names[i],g_suite_scores[i],
               (i<g_current_level) ? "(Unlocked)" : ( (i==g_current_level) ? "(Current Level)" : "(Locked)") );
    }
    printf(" (5 means all done)\n");
}

#ifdef USE_OPENGL_UI
extern void os_gl_start(OSCore* core);
#endif

static void run_os_ui(void){
    OSCore* c=os_init();
    if(!c) {
        printf("[Scheduler] os_init => fail.\n");
        return;
    }
    os_ui_interact(c);
    os_shutdown(c);
}

static void run_ready_ui(void){
    ReadyQueue* rq=ready_create(4);
    if(!rq){
        printf("[Scheduler] ready_create => fail.\n");
        return;
    }
    ready_ui_interact(rq);
    ready_destroy(rq);
}

static void run_worker_ui(void){
    Worker* w=worker_create();
    if(!w){printf("[WorkerUI] creation fail.\n");return;}
    char buf[64];
    printf("HPC threads? [2]: "); fflush(stdout);
    if(!fgets(buf,sizeof(buf),stdin)){worker_destroy(w);return;}
    int ht=parse_int_strtol(buf,2);

    printf("HPC total? [100]: "); fflush(stdout);
    if(!fgets(buf,sizeof(buf),stdin)){worker_destroy(w);return;}
    long hw=parse_int_strtol(buf,100);

    printf("Pipeline stages? [2]: "); fflush(stdout);
    if(!fgets(buf,sizeof(buf),stdin)){worker_destroy(w);return;}
    int ps=parse_int_strtol(buf,2);

    printf("Distributed count? [2]: "); fflush(stdout);
    if(!fgets(buf,sizeof(buf),stdin)){worker_destroy(w);return;}
    int dc=parse_int_strtol(buf,2);

    worker_flags_t fl=WORKER_FLAG_NEW;
    printf("HPC overshadow? [Y/N]: "); fflush(stdout);
    if(fgets(buf,sizeof(buf),stdin)&&(buf[0]=='y'||buf[0]=='Y')) fl|=WORKER_FLAG_HPC;
    printf("Container ephemeral? [Y/N]: "); fflush(stdout);
    if(fgets(buf,sizeof(buf),stdin)&&(buf[0]=='y'||buf[0]=='Y')) fl|=WORKER_FLAG_CONTAINER;
    printf("Pipeline synergy? [Y/N]: "); fflush(stdout);
    if(fgets(buf,sizeof(buf),stdin)&&(buf[0]=='y'||buf[0]=='Y')) fl|=WORKER_FLAG_PIPELINE;
    printf("Distributed? [Y/N]: "); fflush(stdout);
    if(fgets(buf,sizeof(buf),stdin)&&(buf[0]=='y'||buf[0]=='Y')) fl|=WORKER_FLAG_DISTRIB;
    printf("Debug logs? [Y/N]: "); fflush(stdout);
    if(fgets(buf,sizeof(buf),stdin)&&(buf[0]=='y'||buf[0]=='Y')) fl|=WORKER_FLAG_DEBUG;

    printf("Game difficulty? (0=none,1=easy,2=story,3=chall,5=survival): ");
    fflush(stdout);
    if(!fgets(buf,sizeof(buf),stdin)){worker_destroy(w);return;}
    int gdif=parse_int_strtol(buf,0);

    printf("Game base score? [10..100]: ");
    fflush(stdout);
    if(!fgets(buf,sizeof(buf),stdin)){worker_destroy(w);return;}
    int gsco=parse_int_strtol(buf,10);

    if(!worker_configure(w,fl,ht,hw,"/tmp/worker_container_XXXXXX",ps,dc,gdif,gsco)){
        printf("[WorkerUI] config => fail.\n");
        worker_destroy(w);
        return;
    }
    if(!worker_start(w)){
        printf("[WorkerUI] start => fail.\n");
        worker_destroy(w);
        return;
    }
    if(!worker_join(w)){
        printf("[WorkerUI] join => error.\n");
    } else {
        printf("[WorkerUI] HPCres=%ld ephemeral=%s\n",w->hpc_result,
               w->container_ready?w->container_dir:"(none)");
    }
    worker_destroy(w);
}

int main(void){
    printf("\n============================================\n");
    printf("  WELCOME TO THE ADVANCED SCHEDULER/GAME\n");
    printf("  Test progression in order:\n");
    printf("    0 => BASIC\n");
    printf("    1 => NORMAL\n");
    printf("    2 => MODES\n");
    printf("    3 => EDGE\n");
    printf("    4 => HIDDEN\n");
    printf("  Each requires >=60%% success to unlock the next.\n");
    printf("============================================\n");

    while(true){
        printf("\nCurrent test level: %d/5 => %s\n",
               g_current_level,
               (g_current_level>=5)?"ALL DONE":"In Progress...");
        printf("[1] Run NEXT Test in progression\n");
        printf("[2] OS-based concurrency UI\n");
        printf("[3] ReadyQueue concurrency UI\n");
        printf("[4] Single Worker concurrency\n");
        printf("[5] Show partial scoreboard\n");
        printf("[6] Re-run a previously passed test suite\n");
#ifdef USE_OPENGL_UI
        printf("[7] Direct OpenGL-based OS UI\n");
#endif
        printf("[0] Exit\n");
        printf("Your choice: ");
        fflush(stdout);

        char line[32];
        if(!fgets(line,sizeof(line),stdin)){
            printf("[Scheduler] EOF or error.\n");
            break;
        }
        int c=parse_int_strtol(line,-1);
        switch(c){
            case 0:
                printf("[Scheduler] Exiting. Farewell.\n");
                return 1;
            case 1:
                do_next_test();
                break;
            case 2:
                run_os_ui();
                break;
            case 3:
                run_ready_ui();
                break;
            case 4:
                run_worker_ui();
                break;
            case 5:
                do_show_scoreboard();
                break;
            case 6:
                do_rerun_test();
                break;
#ifdef USE_OPENGL_UI
            case 7:{
                OSCore* c2=os_init();
                if(!c2){printf("[Scheduler] init=>fail.\n");break;}
                printf("[Scheduler] launching OpenGL UI...\n");
                os_gl_start(c2);
                os_shutdown(c2);
                break;
            }
#endif
            default:
                printf("Invalid.\n");
                break;
        }
    }
    return 0;
}
