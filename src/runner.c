#include "runner.h"
#include "scheduler.h"
#include "scoreboard.h"
#include "os.h"
#include "safe_calls_library.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <pthread.h>

extern int unlocked_basic, unlocked_normal, unlocked_modes,
           unlocked_edge, unlocked_hidden, unlocked_external;

/* forward declarations of test suite triggers */
void run_basic_tests(int* total,int* passed);
void run_normal_tests(int* total,int* passed);
void run_modes_tests(int* total,int* passed);
void run_edge_tests(int* total,int* passed);
void run_hidden_tests(int* total,int* passed);
void run_external_tests(void);

void run_all_levels(void) {
    /* do them in order if unlocked */
    if (!unlocked_basic) {
        printf("BASIC locked.\n");
        return;
    } else {
        int t=0,p=0;
        run_basic_tests(&t,&p);
        scoreboard_update_basic(t,p);
        scoreboard_save();
    }

    if (!unlocked_normal) {
        printf("NORMAL locked.\n");
        return;
    } else {
        int t=0,p=0;
        run_normal_tests(&t,&p);
        scoreboard_update_normal(t,p);
        scoreboard_save();
    }

    if (!unlocked_modes) {
        printf("MODES locked.\n");
        return;
    } else {
        int t=0,p=0;
        run_modes_tests(&t,&p);
        scoreboard_update_modes(t,p);
        scoreboard_save();
    }

    if (!unlocked_edge) {
        printf("EDGE locked.\n");
        return;
    } else {
        int t=0,p=0;
        run_edge_tests(&t,&p);
        scoreboard_update_edge(t,p);
        scoreboard_save();
    }

    if (!unlocked_hidden) {
        printf("HIDDEN locked.\n");
        return;
    } else {
        int t=0,p=0;
        run_hidden_tests(&t,&p);
        scoreboard_update_hidden(t,p);
        scoreboard_save();
    }
}

/* from external-test.c */
void run_external_tests_menu(void) {
    if (!unlocked_external) return;
    run_external_tests();
}

/* Example advanced child debugging with ptrace. */
static void advanced_debug_child(pid_t pid) {
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        fprintf(stderr,"[Runner] ptrace attach fail (pid=%d): %s\n",
                pid, strerror(errno));
        return;
    }
    waitpid(pid, NULL, 0);
    ptrace(PTRACE_DETACH, pid, NULL, NULL);
}

typedef struct {
    pid_t   pid;
    char*   cmd;
    int     core;
    uint64_t start_ms;
    uint64_t end_ms;
    int     p_out[2];
    int     p_err[2];
    int     p_in[2];
} child_t;

/* All known modes (12). */
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

static pid_t spawn_child(const char* cmd, child_t* ch, int core) {
    pipe(ch->p_out);
    pipe(ch->p_err);
    pipe(ch->p_in);

    pid_t c = fork();
    if (c < 0) {
        fprintf(stderr,"fork() error\n");
        return -1;
    } else if (c == 0) {
        /* Child side */
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
        /* Parent side */
        ch->pid = c;
        ch->cmd = (cmd ? strdup(cmd) : NULL);
        ch->core = core;
        ch->start_ms = os_time();

        close(ch->p_out[1]);
        close(ch->p_err[1]);
        close(ch->p_in[0]);

        /* Send the command + exit */
        dprintf(ch->p_in[1], "%s\nexit\n", cmd ? cmd : "");
        close(ch->p_in[1]);

        /* attach ptrace => advanced debugging example. */
        advanced_debug_child(c);
    }
    return c;
}

void run_shell_commands_concurrently(int count, char** lines, int coreCount, int mode, int allModes) {
    if (count <= 0 || !lines) return;
    if (access("./shell-tp1-implementation", X_OK) != 0) {
        printf("No shell-tp1-implementation found (expected in CWD).\n");
        return;
    }
    int from = 0;
    int to   = (int)(sizeof(modes_arr)/sizeof(modes_arr[0])) - 1;
    if (!allModes) {
        if (mode < 0 || mode > to) {
            printf("Invalid scheduling mode.\n");
            return;
        }
        from = mode;
        to   = mode;
    }

    for (int m = from; m <= to; m++) {
        printf("\n\033[95m$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
        printf("SCHEDULE BLOCK START => %s\n", modeNames[m]);
        printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\033[0m\n");
        usleep(300000);

        child_t* ch = (child_t*)calloc(count, sizeof(child_t));
        if (!ch) return;

        uint64_t global_start = os_time();
        int next_core = 0;

        /* spawn children */
        for (int i=0; i<count; i++) {
            spawn_child(lines[i], &ch[i], next_core);
            printf("\033[92m[time=%llu ms] container=1 core=%d => Launch child#%d cmd=\"%s\"\033[0m\n",
                   (unsigned long long)os_time(),
                   next_core, i+1, lines[i] ? lines[i] : "");
            usleep(300000);
            next_core = (next_core + 1) % coreCount;
        }

        /* wait for each child to finish. */
        for (int i=0; i<count; i++) {
            if (!ch[i].pid) continue;
            waitpid(ch[i].pid, NULL, 0);
            ch[i].end_ms = os_time();
            printf("\033[93m[time=%llu ms] container=1 core=%d => Child#%d ended => cmd=\"%s\" total_duration=%llums\033[0m\n",
                   (unsigned long long)os_time(),
                   ch[i].core, i+1,
                   (ch[i].cmd ? ch[i].cmd : ""),
                   (unsigned long long)(ch[i].end_ms - ch[i].start_ms));
            usleep(300000);
        }

        uint64_t global_end = os_time();
        uint64_t total_time = (global_end > global_start ? (global_end - global_start) : 0ULL);

        /* Drain leftover output, close pipes. */
        for (int i=0; i<count; i++) {
            if (!ch[i].pid) continue;
            char outb[256]={0}, errb[256]={0};
            read(ch[i].p_out[0], outb, sizeof(outb)-1);
            read(ch[i].p_err[0], errb, sizeof(errb)-1);
            close(ch[i].p_out[0]);
            close(ch[i].p_err[0]);
            if (ch[i].cmd) free(ch[i].cmd);
        }
        free(ch);

        printf("\033[96m$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
        printf("SCHEDULE BLOCK END => %s, total_time=%llums\n",
               modeNames[m], (unsigned long long)total_time);
        printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\033[0m\n");
        usleep(400000);
    }
}
