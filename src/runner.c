#include "runner.h"
#include "scheduler.h"
#include "scoreboard.h"
#include "os.h"
#include "safe_calls_library.h"
#include "stats.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <unistd.h>

/* from external-test.c */
void run_external_tests(void);

/* We keep a table of mode enumerations and strings: */
static scheduler_alg_t modes_arr[] = {
    ALG_FIFO, ALG_RR, ALG_CFS, ALG_CFS_SRTF, ALG_BFS,
    ALG_SJF, ALG_STRF, ALG_HRRN, ALG_HRRN_RT, ALG_PRIORITY,
    ALG_HPC_OVERSHADOW, ALG_MLFQ
};
static const char* modeNames[] = {
    "FIFO","RR","CFS","CFS-SRTF","BFS",
    "SJF","STRF","HRRN","HRRN-RT","PRIORITY",
    "HPC-OVER","MLFQ"
};

void run_all_levels(void) {
    printf("[runner] run_all_levels => replaced by menu logic.\n");
}

void run_external_tests_menu(void) {
    if(!scoreboard_is_unlocked(SUITE_EXTERNAL)) {
        return;
    }
    run_external_tests();
}

/* Enhanced ptrace usage for concurrency child. */
static void advanced_debug_child(pid_t pid) {
    if(ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        fprintf(stderr,"[Runner] ptrace attach fail (pid=%d): %s\n",
                pid, strerror(errno));
        return;
    }
    waitpid(pid, NULL, 0);

    if(ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) == -1){
        fprintf(stderr,"[Runner] ptrace singlestep fail: %s\n", strerror(errno));
    }
    waitpid(pid, NULL, 0);

    ptrace(PTRACE_DETACH, pid, NULL, NULL);
}

/* child_t structure to hold concurrency child info. */
typedef struct {
    pid_t   pid;
    char*   cmd;
    int     core;
    uint64_t start_ms;
    int     p_out[2];
    int     p_err[2];
    int     p_in[2];
} child_t;

/* Speed-friendly concurrency sleep. */
static void concurrency_sleep(unsigned int us) {
    int sm = stats_get_speed_mode();
    if(sm == 1) {
        usleep(us/10 + 1);
    } else {
        usleep(us);
    }
}

/* Spawns child => sets up pipes => advanced ptrace => returns pid. */
static pid_t spawn_child(const char* cmd, child_t* ch, int core) {
    pipe(ch->p_out);
    pipe(ch->p_err);
    pipe(ch->p_in);

    pid_t c = fork();
    if (c < 0) {
        fprintf(stderr,"fork() error\n");
        return -1;
    } else if (c == 0) {
        /* Child side => redirect stdio. */
        close(ch->p_out[0]);
        close(ch->p_err[0]);
        close(ch->p_in[0]);
        dup2(ch->p_out[1], STDOUT_FILENO);
        dup2(ch->p_err[1], STDERR_FILENO);
        dup2(ch->p_in[1],  STDIN_FILENO);
        close(ch->p_out[1]);
        close(ch->p_err[1]);
        close(ch->p_in[1]);

        execl("./shell-tp1-implementation", "shell-tp1-implementation", (char*)NULL);
        _exit(127);
    } else {
        /* Parent side => advanced ptrace. */
        ch->pid = c;
        ch->cmd = (cmd ? strdup(cmd) : NULL);
        ch->core = core;
        ch->start_ms = os_time();

        stats_inc_processes_spawned();

        close(ch->p_out[1]);
        close(ch->p_err[1]);
        close(ch->p_in[0]);

        dprintf(ch->p_in[1], "%s\nexit\n", cmd ? cmd : "");
        close(ch->p_in[1]);

        advanced_debug_child(c);
    }
    return c;
}

void run_shell_commands_concurrently(int count,
                                     char** lines,
                                     int coreCount,
                                     int mode,
                                     int allModes) {
    if(count <= 0 || !lines) return;
    if(access("./shell-tp1-implementation", X_OK) != 0){
        printf("No shell-tp1-implementation found (expected in CWD).\n");
        return;
    }

    stats_inc_concurrency_runs();

    int from = 0;
    int to   = (int)(sizeof(modes_arr)/sizeof(modes_arr[0])) - 1;
    if(!allModes) {
        if(mode<0 || mode>to) {
            printf("Invalid scheduling mode.\n");
            return;
        }
        from = mode;
        to   = mode;
    }

    for(int m = from; m <= to; m++) {
        if(os_concurrency_stop_requested()){
            printf("[runner] concurrency stop requested => returning.\n");
            return;
        }

        printf(CLR_MAGENTA "\n╔═════════════════════════════════════════════════════════════╗\n");
        printf("║ SCHEDULE BLOCK START => %s\n", modeNames[m]);
        printf("╚═════════════════════════════════════════════════════════════╝" CLR_RESET "\n");
        concurrency_sleep(300000);

        child_t* ch = (child_t*)calloc(count, sizeof(child_t));
        if(!ch) return;

        uint64_t global_start = os_time();
        int next_core = 0;

        /* spawn children */
        for(int i=0; i<count; i++){
            if(os_concurrency_stop_requested()){
                printf("[runner] concurrency stop during spawn => abort.\n");
                free(ch);
                return;
            }
            spawn_child(lines[i], &ch[i], next_core);
            printf(CLR_GREEN"[time=%llu ms] container=1 core=%d => Launch child#%d cmd=\"%s\"\n"CLR_RESET,
                   (unsigned long long)os_time(),
                   next_core, i+1, lines[i] ? lines[i] : "");
            concurrency_sleep(300000);
            next_core = (next_core + 1) % coreCount;
        }

        /* wait for each child to finish */
        for(int i=0; i<count; i++){
            if(!ch[i].pid) continue;
            if(os_concurrency_stop_requested()){
                printf("[runner] concurrency stop => kill remaining children.\n");
                for(int j=i; j<count; j++){
                    if(ch[j].pid) {
                        kill(ch[j].pid, SIGKILL);
                    }
                }
                break;
            }
            waitpid(ch[i].pid, NULL, 0);
            uint64_t end_ms = os_time();
            printf(CLR_YELLOW"[time=%llu ms] container=1 core=%d => Child#%d ended => cmd=\"%s\" total_duration=%llums\n"CLR_RESET,
                   (unsigned long long)os_time(),
                   ch[i].core, i+1,
                   (ch[i].cmd ? ch[i].cmd : ""),
                   (unsigned long long)(end_ms - ch[i].start_ms));
            concurrency_sleep(300000);
        }

        uint64_t global_end = os_time();
        uint64_t total_time = (global_end > global_start) ? (global_end - global_start) : 0ULL;

        /* Drain leftover output, close pipes. */
        for(int i=0; i<count; i++){
            if(!ch[i].pid) continue;
            char outb[256]={0}, errb[256]={0};
            read(ch[i].p_out[0], outb, sizeof(outb)-1);
            read(ch[i].p_err[0], errb, sizeof(errb)-1);
            close(ch[i].p_out[0]);
            close(ch[i].p_err[0]);
            if(ch[i].cmd) free(ch[i].cmd);
        }
        free(ch);

        printf(CLR_MAGENTA"╔═════════════════════════════════════════════════════════════╗\n");
        printf("║ SCHEDULE BLOCK END => %s, total_time=%llums\n",
               modeNames[m], (unsigned long long)total_time);
        printf("╚═════════════════════════════════════════════════════════════╝"CLR_RESET"\n");
        concurrency_sleep(400000);
    }
}
