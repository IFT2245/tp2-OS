#include "runner.h"
#include "scoreboard.h"
#include "stats.h"
#include "safe_calls_library.h"
#include "os.h"
#include "scheduler.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/*
  We'll maintain a lookup for scheduler_alg_t => string name
  to produce "SCHEDULE BLOCK => <Name>" in concurrency logs, if needed.
*/
static const char* scheduler_alg_to_str(scheduler_alg_t alg) {
    switch(alg) {
        case ALG_FIFO:          return "FIFO";
        case ALG_RR:            return "Round Robin";
        case ALG_CFS:           return "CFS";
        case ALG_CFS_SRTF:      return "CFS-SRTF";
        case ALG_BFS:           return "BFS";
        case ALG_SJF:           return "SJF";
        case ALG_STRF:          return "STRF";
        case ALG_HRRN:          return "HRRN";
        case ALG_HRRN_RT:       return "HRRN-RT";
        case ALG_PRIORITY:      return "PRIORITY";
        case ALG_HPC_OVERSHADOW:return "HPC-OVER";
        case ALG_MLFQ:          return "MLFQ";
        default:                return "Unknown";
    }
}

/* forward declarations for test suite runners */
extern void run_basic_tests(int* total,int* passed);
extern void run_normal_tests(int* total,int* passed);
extern void run_modes_tests(int* total,int* passed);
extern void run_edge_tests(int* total,int* passed);
extern void run_hidden_tests(int* total,int* passed);

/* new external tests approach: */
extern void run_external_tests(int* total,int* passed);

/* single-test extern: */
extern int basic_test_count(void);
extern void basic_test_run_single(int idx, int* pass_out);

extern int normal_test_count(void);
extern void normal_test_run_single(int idx, int* pass_out);

extern int modes_test_count(void);
extern void modes_test_run_single(int idx, int* pass_out);

extern int edge_test_count(void);
extern void edge_test_run_single(int idx, int* pass_out);

extern int hidden_test_count(void);
extern void hidden_test_run_single(int idx, int* pass_out);

extern int external_test_count(void);
extern void external_test_run_single(int idx,int* pass_out);


/* ----------------------------------------------------------------
   run_entire_suite => same pattern for each suite
   Then scoreboard_update_* after we get total/passed.
---------------------------------------------------------------- */
void run_entire_suite(scoreboard_suite_t suite) {
    switch(suite) {
    case SUITE_BASIC: {
        int t=0, p=0;
        printf("\n[Running BASIC suite...]\n");
        run_basic_tests(&t,&p);
        scoreboard_update_basic(t,p);
        scoreboard_save();
        break;
    }
    case SUITE_NORMAL: {
        int t=0, p=0;
        printf("\n[Running NORMAL suite...]\n");
        run_normal_tests(&t,&p);
        scoreboard_update_normal(t,p);
        scoreboard_save();
        break;
    }
    case SUITE_EXTERNAL: {
        int t=0, p=0;
        printf("\n[Running EXTERNAL suite...]\n");
        run_external_tests(&t, &p);
        scoreboard_update_external(t,p);
        scoreboard_save();
        break;
    }
    case SUITE_MODES: {
        int t=0, p=0;
        printf("\n[Running MODES suite...]\n");
        run_modes_tests(&t,&p);
        scoreboard_update_modes(t,p);
        scoreboard_save();
        break;
    }
    case SUITE_EDGE: {
        int t=0, p=0;
        printf("\n[Running EDGE suite...]\n");
        run_edge_tests(&t,&p);
        scoreboard_update_edge(t,p);
        scoreboard_save();
        break;
    }
    case SUITE_HIDDEN: {
        int t=0, p=0;
        printf("\n[Running HIDDEN suite...]\n");
        run_hidden_tests(&t,&p);
        scoreboard_update_hidden(t,p);
        scoreboard_save();
        break;
    }
    default:
        break;
    }
}

/*
   run_external_tests_menu => original code had "run_external_tests()",
   but now we don't store results here, we do that in run_entire_suite(SUITE_EXTERNAL).
   If the menu just wants to run external suite, it can either call run_entire_suite or
   do partial logic. We'll keep a function to run it "on-demand," though.
*/
void run_external_tests_menu(void) {
    int t=0, p=0;
    run_external_tests(&t, &p);
    scoreboard_update_external(t,p);
    scoreboard_save();
}

/*
   run_single_test_in_suite => used by the "Run Single Test" menu item
*/
void run_single_test_in_suite(scoreboard_suite_t chosen) {
    int count = 0;
    switch(chosen) {
        case SUITE_BASIC:   count = basic_test_count();   break;
        case SUITE_NORMAL:  count = normal_test_count();  break;
        case SUITE_MODES:   count = modes_test_count();   break;
        case SUITE_EDGE:    count = edge_test_count();    break;
        case SUITE_HIDDEN:  count = hidden_test_count();  break;
        case SUITE_EXTERNAL:count = external_test_count();break;
        default: break;
    }
    if(count<=0) {
        printf("No tests found in that suite or suite missing.\n");
        return;
    }

    printf("\nWhich single test do you want to run (1..%d)? ", count);
    char buf[256];
    if(!fgets(buf, sizeof(buf), stdin)) {
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
    int pick = parse_int_strtol(buf, -1);
    if(pick<1 || pick>count) {
        printf("Invalid test index.\n");
        return;
    }

    int passResult = 0; /* 0 => fail, 1 => pass */
    switch(chosen){
        case SUITE_BASIC:
            basic_test_run_single(pick-1, &passResult);
            scoreboard_update_basic(1, passResult);
            scoreboard_save();
            break;
        case SUITE_NORMAL:
            normal_test_run_single(pick-1, &passResult);
            scoreboard_update_normal(1, passResult);
            scoreboard_save();
            break;
        case SUITE_MODES:
            modes_test_run_single(pick-1, &passResult);
            scoreboard_update_modes(1, passResult);
            scoreboard_save();
            break;
        case SUITE_EDGE:
            edge_test_run_single(pick-1, &passResult);
            scoreboard_update_edge(1, passResult);
            scoreboard_save();
            break;
        case SUITE_HIDDEN:
            hidden_test_run_single(pick-1, &passResult);
            scoreboard_update_hidden(1, passResult);
            scoreboard_save();
            break;
        case SUITE_EXTERNAL:
            external_test_run_single(pick-1, &passResult);
            scoreboard_update_external(1, passResult);
            scoreboard_save();
            break;
        default:
            break;
    }
}

/* ----------------------------------------------------------------
   Shell concurrency logic
   If speed=FAST => skip block prints, else show them.
   If mode=-1 => run all from 0..11, else single mode.
---------------------------------------------------------------- */
static const char* scheduler_alg_to_str2(int mode) {
    switch(mode) {
        case 0:  return "FIFO";
        case 1:  return "Round Robin";
        case 2:  return "CFS";
        case 3:  return "CFS-SRTF";
        case 4:  return "BFS";
        case 5:  return "SJF";
        case 6:  return "STRF";
        case 7:  return "HRRN";
        case 8:  return "HRRN-RT";
        case 9:  return "PRIORITY";
        case 10: return "HPC-OVER";
        case 11: return "MLFQ";
        // overlay
        default: return "UNKNOWN";
    }
}

static void concurrency_log(const char* fmt, ...) __attribute__((format(printf,1,2))); // - The `__attribute__((format(printf,1,2)))` is a GCC attribute that tells the compiler to type-check the format string like printf. The numbers `1,2` indicate that the format string is parameter 1, and the variadic arguments start at parameter
static void concurrency_log(const char* fmt, ...)
{
    if(stats_get_speed_mode()==1) {
        /* FAST => skip printing */
        return;
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static int check_shell_binary(void) {
    if(access("../../shell-tp1-implementation", X_OK) != 0){
        printf(CLR_RED "\n╔══════════════════════════════════════════════╗\n");
        printf("║shell-tp1-implementation NOT FOUND in .       ║\n");
        printf("║External concurrency test cannot run.         ║\n");
        printf("╚══════════════════════════════════════════════╝\n" CLR_RESET);
        return 0;
    }
    return 1;
}

void run_shell_commands_concurrently(int count,
                                     char** lines,
                                     int coreCount,
                                     int mode,
                                     int allModes)
{
    if(count<=0 || !lines) return;
    if(!check_shell_binary()) {
        /* if missing => just fail quietly or print the message above => done */
        return;
    }

    int from=0, to=11;
    if(!allModes) {
        if(mode<0 || mode>11) {
            concurrency_log("\n[Invalid scheduling mode => skipping concurrency.]\n");
            return;
        }
        from = mode;
        to   = mode;
    }

    for(int m=from; m<=to; m++){
        const char* alg_name = scheduler_alg_to_str2(m);

        if(stats_get_speed_mode()==0) {
            concurrency_log(CLR_MAGENTA "\n╔═════════════════════════════════════════════════════════════╗\n");
            concurrency_log("║ EXTERNAL SCHEDULE BLOCK START => %s\n", alg_name);
            concurrency_log("╚═════════════════════════════════════════════════════════════╝\n" CLR_RESET);
        }

        pid_t* pids = (pid_t*)calloc(count, sizeof(pid_t));
        if(!pids) return;
        for(int i=0; i<count; i++){
            concurrency_log(CLR_GREEN "[time=%llu ms] core=%d => Launch child#%d cmd=\"%s\"\n" CLR_RESET,
                            (unsigned long long)os_time(),
                            (i % coreCount), i+1,
                            lines[i]?lines[i]:"");
            /* We'll do a simple fork + send command to shell's stdin,
               as a minimal approach. */
            int pipefd[2];
            if(pipe(pipefd)==-1) {
                perror("pipe");
                continue;
            }
            pid_t c = fork();
            if(c<0) {
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                continue;
            }
            else if(c==0){
                /* child => read from pipefd[0] as STDIN, then execl the shell. */
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);
                execl("./shell-tp1-implementation",
                      "shell-tp1-implementation", (char*)NULL);
                _exit(1);
            }
            else {
                /* parent */
                pids[i]=c;
                close(pipefd[0]);
                dprintf(pipefd[1], "%s\nexit\n", lines[i]?lines[i]:"");
                close(pipefd[1]);
            }
        }

        /* wait for them */
        for(int i=0; i<count; i++){
            if(!pids[i]) continue;
            waitpid(pids[i], NULL, 0);
            concurrency_log(CLR_YELLOW "[time=%llu ms] => Child#%d ended => cmd=\"%s\"\n" CLR_RESET,
                            (unsigned long long)os_time(),
                            i+1,
                            lines[i]?lines[i]:"");
        }
        free(pids);

        if(stats_get_speed_mode()==0){
            concurrency_log(CLR_MAGENTA "╔═════════════════════════════════════════════════════════════╗\n");
            concurrency_log("║ EXTERNAL SCHEDULE BLOCK END => %s\n", alg_name);
            concurrency_log("╚═════════════════════════════════════════════════════════════╝\n" CLR_RESET);
        }
    }
}



void run_concurrency_with_containers(void) {
    /* Container #1 => 2 normal processes, no HPC, ALG_FIFO, 2 cores */
    process_t list1[2];
    init_process(&list1[0], 3, 5, 0);
    init_process(&list1[1], 5, 6, 0);
    container_t c1;
    container_init(&c1, 2, false, ALG_FIFO, ALG_NONE,
                   list1, 2,
                   NULL, 0);

    /* Container #2 => HPC enabled => HPC alg=ALG_HPC, main=ALG_RR, etc. */
    process_t normal2[2];
    init_process(&normal2[0], 4, 10, 0);
    init_process(&normal2[1], 2, 5, 0);

    process_t hpc2[1];
    init_process(&hpc2[0], 5, 1, 0);

    container_t c2;
    container_init(&c2, 2, true, ALG_RR, ALG_HPC,
                   normal2, 2,
                   hpc2,    1);

    container_t containers[2] = { c1, c2 };
    orchestrator_run(containers, 2);
}