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
#include <signal.h>
#include <stdarg.h>

#include "../test/external-test.h"  /* so we can call run_external_tests() */


/* Private: map of scheduling mode enums to string names. */
static const char* modeNames[] = {
    "FIFO","RR","CFS","CFS-SRTF","BFS",
    "SJF","STRF","HRRN","HRRN-RT","PRIORITY",
    "HPC-OVER","MLFQ"
};

/* We no longer implement run_all_levels(); replaced by menu. */
void run_all_levels(void) {
    printf("[runner] run_all_levels => replaced by main menu logic.\n");
}

/*
  Runs external tests if unlocked.
*/
void run_external_tests_menu(void) {
    if(!scoreboard_is_unlocked(SUITE_EXTERNAL)) {
        return;
    }
    run_external_tests();
}

/*
  Child info structure for concurrency tests.
*/
typedef struct {
    pid_t   pid;
    char*   cmd;
    int     core;
    uint64_t start_ms;
    int     p_out[2];
    int     p_err[2];
    int     p_in[2];
} child_t;

/* For concurrency logs, skip if speed=FAST. */
static void concurrency_log(const char* fmt, ...) {
    if(stats_get_speed_mode() == 1) {
        return; /* FAST => skip printing to accelerate */
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/* Use ptrace to attach and single-step for demonstration. */
static void advanced_debug_child(pid_t pid) {
    int sm = stats_get_speed_mode();
    if(ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        if(sm==0) {
            fprintf(stderr,"[Runner] ptrace attach fail (pid=%d): %s\n",
                    pid, strerror(errno));
        }
        return;
    }
    waitpid(pid, NULL, 0);

    /* Single step */
    if(ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) == -1) {
        if(sm==0) {
            fprintf(stderr,"[Runner] ptrace singlestep fail: %s\n", strerror(errno));
        }
    }
    waitpid(pid, NULL, 0);

    /* Syscall step */
    if(ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) {
        if(sm==0) {
            fprintf(stderr,"[Runner] ptrace syscall fail: %s\n", strerror(errno));
        }
    }
    waitpid(pid, NULL, 0);

    ptrace(PTRACE_DETACH, pid, NULL, NULL);
}

/*
  Spawns a child running the external "shell-tp1-implementation" binary,
  sets up pipes, sends the command, attaches with ptrace for one step, etc.
*/
static pid_t spawn_child(const char* cmd, child_t* ch, int core) {
    pipe(ch->p_out);
    pipe(ch->p_err);
    pipe(ch->p_in);

    pid_t c = fork();
    if (c < 0) {
        fprintf(stderr,"fork() error\n");
        return -1;
    } else if (c == 0) {
        close(ch->p_out[0]);
        close(ch->p_err[0]);
        close(ch->p_in[0]);

        dup2(ch->p_out[1], STDOUT_FILENO);
        dup2(ch->p_err[1], STDERR_FILENO);
        dup2(ch->p_in[1],  STDIN_FILENO);

        close(ch->p_out[1]);
        close(ch->p_err[1]);
        close(ch->p_in[1]);

        execl("../../shell-tp1-implementation", "shell-tp1-implementation", (char*)NULL);
        _exit(0);
    } else {
        ch->pid = c;
        ch->cmd = (cmd ? strdup(cmd) : NULL);
        ch->core = core;
        ch->start_ms = os_time();
        stats_inc_processes_spawned();

        close(ch->p_out[1]);
        close(ch->p_err[1]);
        close(ch->p_in[0]);

        /* send the command + exit to child's stdin */
        dprintf(ch->p_in[1], "%s\nexit\n", cmd ? cmd : "");
        close(ch->p_in[1]);

        advanced_debug_child(c);
    }
    return c;
}

/*
  Run shell commands concurrently with a chosen scheduling mode
  or all modes. We produce schedule blocks around each mode run
  in normal mode. In fast mode, we skip the schedule block prints.
*/
void run_shell_commands_concurrently(int count,
                                     char** lines,
                                     int coreCount,
                                     int mode,
                                     int allModes)
{
    if(count <= 0 || !lines) return;

    /* check if external shell is present */
    if(access("./shell-tp1-implementation", X_OK) != 0){
        printf(CLR_RED "\n╔══════════════════════════════════════════════╗\n");
        printf("║shell-tp1-implementation NOT FOUND in root dir║\n");
        printf("║Concurrency test cannot run.                  ║\n");
        printf("╚══════════════════════════════════════════════╝\n" CLR_RESET);
        return;
    }

    stats_inc_concurrency_runs();

    /* Overall concurrency block */
    if(stats_get_speed_mode()==0) {
        printf(CLR_MAGENTA "\n╔═══════════════════════════════════════════════════════════════╗\n");
        printf(             "║                  Shell Commands SCHEDULE BLOCK                ║\n");
        printf(             "╚═══════════════════════════════════════════════════════════════╝\n" CLR_RESET);
    }

    /* Single mode => from=mode..mode, All => from=0..11 */
    int from = 0;
    int to   = 11;
    if(!allModes) {
        if(mode<0 || mode>to) {
            /* invalid mode => skip */
            if(stats_get_speed_mode()==0) {
                printf(CLR_MAGENTA "\n╔═════════════════════════════════════════════════════════════╗\n");
                printf("║ CONCURRENCY BLOCK => Invalid mode specified                 ║\n");
                printf("╚═════════════════════════════════════════════════════════════╝\n" CLR_RESET);
            }
            return;
        }
        from = mode;
        to   = mode;
    }

    /* track concurrency commands run */
    stats_inc_concurrency_commands(count * ((allModes)? (to-from+1):1));

    for(int m = from; m <= to; m++) {
        if(os_concurrency_stop_requested()) {
            concurrency_log(CLR_MAGENTA "\n╔═════════════════════════════════════════════════════════════╗\n");
            concurrency_log("║ CONCURRENCY STOP REQUESTED => returning...                  ║\n");
            concurrency_log("╚═════════════════════════════════════════════════════════════╝\n" CLR_RESET);
            break;
        }

        /* Show mode name if normal speed */
        if(stats_get_speed_mode()==0){
            concurrency_log(CLR_MAGENTA "\n╔═════════════════════════════════════════════════════════════╗\n");
            concurrency_log("║ SCHEDULE BLOCK START => %s\n", modeNames[m]);
            concurrency_log("╚═════════════════════════════════════════════════════════════╝" CLR_RESET "\n");
        }

        child_t* ch = (child_t*)calloc(count, sizeof(child_t));
        if(!ch) return;

        uint64_t global_start = os_time();
        int next_core = 0;

        /* spawn each child process */
        for(int i=0; i<count; i++){
            if(os_concurrency_stop_requested()){
                concurrency_log("[runner] concurrency stop => skip remaining spawns.\n");
                break;
            }
            spawn_child(lines[i], &ch[i], next_core);

            concurrency_log(CLR_GREEN"[time=%llu ms] container=1 core=%d => Launch child#%d cmd=\"%s\"\n"CLR_RESET,
                            (unsigned long long)os_time(),
                            next_core, i+1, lines[i] ? lines[i] : "");
            next_core = (next_core + 1) % coreCount;
        }

        /* wait each child. If stop requested => kill the rest. */
        for(int i=0; i<count; i++){
            if(!ch[i].pid) continue;
            if(os_concurrency_stop_requested()){
                concurrency_log("[runner] concurrency stop => kill remaining children.\n");
                kill(ch[i].pid, SIGKILL);
                continue;
            }
            waitpid(ch[i].pid, NULL, 0);
            uint64_t end_ms = os_time();
            concurrency_log(CLR_YELLOW"[time=%llu ms] container=1 core=%d => Child#%d ended => cmd=\"%s\" total_duration=%llums\n"CLR_RESET,
                            (unsigned long long)os_time(),
                            ch[i].core, i+1,
                            (ch[i].cmd ? ch[i].cmd : ""),
                            (unsigned long long)(end_ms - ch[i].start_ms));
        }

        uint64_t global_end = os_time();
        uint64_t total_time = (global_end > global_start) ? (global_end - global_start) : 0ULL;

        /* read from child pipes to consume output, then free */
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

        if(stats_get_speed_mode()==0){
            concurrency_log(CLR_MAGENTA"╔═════════════════════════════════════════════════════════════╗\n");
            concurrency_log("║ SCHEDULE BLOCK END => %s, total_time=%llums\n",
                             modeNames[m], (unsigned long long)total_time);
            concurrency_log("╚═════════════════════════════════════════════════════════════╝"CLR_RESET"\n");
        }
    }

    if(stats_get_speed_mode()==0){
        printf(CLR_MAGENTA "\n╔═══════════════════════════════════════════════════════════════╗\n");
        printf(             "║  END CONCURRENCY SCHEDULE BLOCK                               ║\n");
        printf(             "╚═══════════════════════════════════════════════════════════════╝\n" CLR_RESET);
    }

    set_os_concurrency_stop_flag(0);
}
