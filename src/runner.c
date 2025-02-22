#include "runner.h"
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
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

/* Global flags controlling unlock logic. */
int unlocked_basic    = 1;  /* Basic always unlocked from start */
int unlocked_normal   = 0;
int unlocked_external = 0;
int unlocked_modes    = 0;
int unlocked_edge     = 0;
int unlocked_hidden   = 0;

/* A scoreboard struct that holds test results globally. */
static scoreboard_t gSB = {
    0,0,0,0,0,0,0,0,0,0,0,0,
    0.0,0.0,0.0,0.0
};

/* Internal helper to update all percentages and unlock flags. */
static void update_unlocks(void){
    gSB.basic_percent   = (gSB.basic_total>0)   ? (gSB.basic_pass   *100.0 / (double)gSB.basic_total)   : 0.0;
    gSB.normal_percent  = (gSB.normal_total>0)  ? (gSB.normal_pass  *100.0 / (double)gSB.normal_total)  : 0.0;
    gSB.modes_percent   = (gSB.modes_total>0)   ? (gSB.modes_pass   *100.0 / (double)gSB.modes_total)   : 0.0;
    gSB.edge_percent    = (gSB.edge_total>0)    ? (gSB.edge_pass    *100.0 / (double)gSB.edge_total)    : 0.0;
    gSB.hidden_percent  = (gSB.hidden_total>0)  ? (gSB.hidden_pass  *100.0 / (double)gSB.hidden_total)  : 0.0;

    /* Basic => unlock Normal + External */
    if(gSB.basic_percent >= gSB.pass_threshold){
        unlocked_normal   = 1;
        unlocked_external = 1;
    }
    /* Normal => unlock Modes */
    if(gSB.normal_percent >= gSB.pass_threshold){
        unlocked_modes = 1;
    }
    /* Modes => unlock Edge */
    if(gSB.modes_percent >= gSB.pass_threshold){
        unlocked_edge = 1;
    }
    /* Edge => unlock Hidden */
    if(gSB.edge_percent >= gSB.pass_threshold){
        unlocked_hidden = 1;
    }
}

/* Runs all levels from basic up to hidden if unlocked, storing results in scoreboard. */
void run_all_levels(void){
    /* BASIC */
    if(!unlocked_basic){
        printf("\nBASIC locked. (Should not happen if starting unlocked.)\n");
        return;
    }
    printf("\n\033[95m====================================\nStarting BASIC\n====================================\033[0m\n");
    run_basic_tests(&gSB.basic_total, &gSB.basic_pass);
    double br = (gSB.basic_total>0) ? (gSB.basic_pass*100.0 / (double)gSB.basic_total) : 0.0;
    printf("\nBASIC Summary: %d total, %d passed, %.1f%%\n", gSB.basic_total, gSB.basic_pass, br);
    update_unlocks();
    if(!unlocked_normal){
        printf("\n\033[90mNORMAL locked. Stopping.\033[0m\n");
        return;
    }

    /* NORMAL */
    printf("\n\033[95m====================================\nStarting NORMAL\n====================================\033[0m\n");
    run_normal_tests(&gSB.normal_total, &gSB.normal_pass);
    double nr = (gSB.normal_total>0)?(gSB.normal_pass*100.0/(double)gSB.normal_total):0.0;
    printf("NORMAL => %d/%d passed (%.1f%%)\n", gSB.normal_pass, gSB.normal_total, nr);
    update_unlocks();
    if(!unlocked_modes){
        printf("\n\033[90mMODES locked. Stopping.\033[0m\n");
        return;
    }

    /* MODES */
    printf("\n\033[95m====================================\nStarting MODES\n====================================\033[0m\n");
    run_modes_tests(&gSB.modes_total, &gSB.modes_pass);
    double mr = (gSB.modes_total>0)?(gSB.modes_pass*100.0/(double)gSB.modes_total):0.0;
    printf("MODES => %d/%d passed (%.1f%%)\n", gSB.modes_pass, gSB.modes_total, mr);
    update_unlocks();
    if(!unlocked_edge){
        printf("\n\033[90mEDGE locked. Stopping.\033[0m\n");
        return;
    }

    /* EDGE */
    printf("\n\033[95m====================================\nStarting EDGE\n====================================\033[0m\n");
    run_edge_tests(&gSB.edge_total, &gSB.edge_pass);
    double er = (gSB.edge_total>0)?(gSB.edge_pass*100.0/(double)gSB.edge_total):0.0;
    printf("EDGE => %d/%d passed (%.1f%%)\n", gSB.edge_pass, gSB.edge_total, er);
    update_unlocks();
    if(!unlocked_hidden){
        printf("\n\033[90mHIDDEN locked. Stopping.\033[0m\n");
        return;
    }

    /* HIDDEN */
    printf("\n\033[95m====================================\nStarting HIDDEN\n====================================\033[0m\n");
    run_hidden_tests(&gSB.hidden_total, &gSB.hidden_pass);
    double hr = (gSB.hidden_total>0)?(gSB.hidden_pass*100.0/(double)gSB.hidden_total):0.0;
    printf("\n[HIDDEN] %d tests, %d passed, %.1f%%\n", gSB.hidden_total, gSB.hidden_pass, hr);
    printf("\n\033[92mAll possible levels executed.\033[0m\n");
    update_unlocks();
}

void run_external_tests_menu(void){
    if(!unlocked_external){
        printf("\n\033[93mExternal tests locked.\033[0m\n");
        return;
    }
    run_external_tests();
}

/* Return a copy of scoreboard. */
void get_scoreboard(scoreboard_t* out){
    if(!out) return;
    *out = gSB;
}

/*
   Enhances concurrency output with:
   - color-coded overlap bars
   - partial stats
   We also fix the double ENTER bug by not prompting for extra input afterwards.
*/
void run_shell_commands_concurrently(int count, char** lines, int coreCount){
    if(count <= 0 || !lines) return;

    /* Simple structure to store concurrency stats. We do an ASCII timeline. */
    typedef struct {
        int    p_out[2];
        int    p_err[2];
        int    p_in[2];
        pid_t  pid;
        char*  cmd;
        uint64_t start_ms;
        uint64_t end_ms;
        int    core_assigned;
    } child_info_t;

    /* Minimal stats per core. */
    typedef struct {
        unsigned long long total_exec_ms;
        unsigned int processes_ran;
    } core_stats_t;

    core_stats_t* cstats = (core_stats_t*)calloc((size_t)coreCount, sizeof(core_stats_t));
    if(!cstats){
        fprintf(stderr,"[runner] Not enough memory for cstats.\n");
        return;
    }
    child_info_t* children = (child_info_t*)calloc((size_t)count, sizeof(child_info_t));
    if(!children){
        free(cstats);
        fprintf(stderr,"[runner] Not enough memory for children.\n");
        return;
    }

    /* Check if user-compiled shell is present. We'll still attempt to run. */
    if(access("./shell-tp1-implementation", X_OK) != 0){
        printf("[runner] shell-tp1-implementation not found or not executable.\n");
        free(children);
        free(cstats);
        return;
    }

    unsigned long long global_start = os_time();
    printf("[runner] Starting concurrency with %d processes, on %d cores\n", count, coreCount);

    int next_core = 0;
    for(int i=0; i<count; i++){
        if(!lines[i] || !*lines[i]){
            printf("[runner] Skipped empty command line for process #%d\n", i+1);
            continue;
        }
        if(pipe(children[i].p_out) < 0 || pipe(children[i].p_err) < 0 || pipe(children[i].p_in) < 0){
            fprintf(stderr,"[runner] pipe() failed for process #%d\n", i+1);
            continue;
        }
        pid_t c = fork();
        if(c < 0){
            fprintf(stderr,"[runner] fork() failed for process #%d\n", i+1);
        } else if(c == 0){
            /* Child */
            close(children[i].p_out[0]);
            close(children[i].p_err[0]);
            close(children[i].p_in[1]);
            dup2(children[i].p_out[1], STDOUT_FILENO);
            dup2(children[i].p_err[1], STDERR_FILENO);
            dup2(children[i].p_in[0],  STDIN_FILENO);
            close(children[i].p_out[1]);
            close(children[i].p_err[1]);
            close(children[i].p_in[0]);

            /* We could wrap the command with an external 'time' tool, but let's keep it simpler here. */
            execl("./shell-tp1-implementation", "shell-tp1-implementation", (char*)NULL);
            _exit(127);
        } else {
            /* Parent */
            children[i].pid          = c;
            children[i].cmd          = lines[i];
            children[i].start_ms     = os_time();
            children[i].core_assigned= next_core;

            close(children[i].p_out[1]);
            close(children[i].p_err[1]);
            close(children[i].p_in[0]);

            /* Send the user command + exit, then close input. */
            dprintf(children[i].p_in[1], "%s\nexit\n", lines[i]);
            close(children[i].p_in[1]);

            printf("[runner] Child #%d (pid=%d) started, command='%s', core=%d\n",
                   i+1, (int)c, lines[i], children[i].core_assigned);

            next_core = (next_core + 1) % coreCount;
        }
    }

    /* Wait for all child processes. */
    for(int i=0; i<count; i++){
        if(!children[i].cmd || !children[i].pid) continue;
        int status = 0;
        waitpid(children[i].pid, &status, 0);
        children[i].end_ms = os_time();
        unsigned long long took = children[i].end_ms - children[i].start_ms;
        cstats[ children[i].core_assigned ].total_exec_ms += took;
        cstats[ children[i].core_assigned ].processes_ran++;
        printf("[runner] Child #%d (pid=%d) ended, status=%d, took=%llu ms\n",
               i+1, (int)children[i].pid, status, took);
    }

    /* Show concurrency results with timeline-like approach. */
    unsigned long long global_end = os_time();
    unsigned long long total_time = global_end - global_start;
    if(total_time < 1) total_time = 1;

    printf("\n=== Concurrency Overlap & Logs ===\n");
    for(int i=0; i<count; i++){
        if(!children[i].cmd || !children[i].pid) continue;
        char buf_out[4096] = {0}, buf_err[4096] = {0};
        ssize_t n_o = read(children[i].p_out[0], buf_out, sizeof(buf_out)-1);
        if(n_o > 0) buf_out[n_o] = '\0';
        close(children[i].p_out[0]);

        ssize_t n_e = read(children[i].p_err[0], buf_err, sizeof(buf_err)-1);
        if(n_e > 0) buf_err[n_e] = '\0';
        close(children[i].p_err[0]);

        unsigned long long took = children[i].end_ms - children[i].start_ms;
        if(took < 1) took = 1;

        /* Simple timeline bar. Each char ~ 50 ms for demonstration. */
        unsigned long long bar_len = took / 50ULL;
        if(bar_len < 1) bar_len = 1;
        char timeline[256] = {0};
        if(bar_len >= sizeof(timeline)) bar_len = sizeof(timeline) - 2;
        for(unsigned long long k=0; k<bar_len; k++){
            timeline[k] = '=';
        }

        /* Color per core (just cycle through a small palette). */
        static const char* core_colors[] = {
            "\033[91m", "\033[92m", "\033[93m", "\033[94m",
            "\033[95m", "\033[96m", "\033[97m", "\033[90m"
        };
        const char* color = core_colors[ children[i].core_assigned % 8 ];

        printf("\nProcess #%d (pid=%d) Core=%d Elapsed=%llu ms\n", i+1, (int)children[i].pid, children[i].core_assigned, took);
        printf("Command: %s\n", children[i].cmd);
        printf("Timeline: %s%s%s\n", color, timeline, "\033[0m");
        printf("----- STDOUT -----\n%s\n----- STDERR -----\n%s\n", buf_out, buf_err);
    }

    /* Stats summary */
    printf("\nTotal concurrency duration: %llu ms\n", total_time);
    printf("┌─────────────────────────────────────┐\n");
    printf("│ Scheduling & Execution Statistics │\n");
    printf("├─────────────────────────────────────┤\n");
    printf("│   Start Time: %llu ms\n", global_start);
    printf("│   End Time:   %llu ms\n", global_end);
    printf("│   Duration:   %llu ms\n", total_time);
    for(int c=0; c<coreCount; c++){
        printf("│   Core #%d => ran %u processes, total_exec=%llu ms\n",
               c, cstats[c].processes_ran, cstats[c].total_exec_ms);
    }
    printf("└─────────────────────────────────────┘\n");
    printf("[runner] Concurrency finished.\n");

    free(children);
    free(cstats);
}
