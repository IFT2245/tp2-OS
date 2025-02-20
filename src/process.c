#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

static inline void proc_set_flag(Process* p, process_flags_t f) { p->flags |= f; }
static inline bool proc_has_flag(const Process* p, process_flags_t f) { return (p->flags & f) != 0; }

typedef struct {
    long start;
    long end;
    long partial;
} HPCPartialArg;

static void* proc_hpc_partial_sum(void* arg) {
    HPCPartialArg* h = (HPCPartialArg*)arg;
    long sumv = 0;
    for (long i = h->start; i < h->end; i++) {
        sumv += i;
    }
    h->partial = sumv;
    return arg;
}

static bool create_ephemeral_dir(char* path, size_t size) {
    if (!path || size < 12) return false;
    if (!strstr(path, "XXXXXX")) {
        size_t len = strlen(path);
        if (len + 6 < size) {
            strcat(path, "XXXXXX");
        } else {
            snprintf(path, size, "/tmp/process_container_XXXXXX");
        }
    }
    char* r = mkdtemp(path);
    return (r != NULL);
}

Process* process_create(void) {
    Process* p = (Process*)calloc(1, sizeof(Process));
    if (!p) return NULL;
    p->flags = PROC_FLAG_NEW | PROC_FLAG_COMPAT_BREAK;
    p->pid = -1;
    p->tid = 0;
    p->priority = 0;
    p->hpc_work = 100;
    p->completed = false;
    snprintf(p->container_dir, sizeof(p->container_dir), "/tmp/process_container_XXXXXX");
    p->pipeline_stages = 3;
    p->distributed_count = 2;
    p->partial_result = 0;
    p->game_score = 0;
    p->game_difficulty = 0;
    p->parent = NULL;
    return p;
}

bool process_configure(Process* proc, process_flags_t flags, int priority,
                       long hpc_work, const char* container_path,
                       int pipeline_stages, int distributed_count,
                       int game_mode_score, int game_mode_diff) {
    if (!proc) return false;
    proc->flags |= flags;
    proc->priority = priority;
    if (hpc_work > 0) proc->hpc_work = hpc_work;
    if (container_path && *container_path) {
        snprintf(proc->container_dir, sizeof(proc->container_dir), "%s", container_path);
    }
    if (pipeline_stages >= 1) proc->pipeline_stages = pipeline_stages;
    if (distributed_count >= 0) proc->distributed_count = distributed_count;
    proc->game_score = game_mode_score;
    proc->game_difficulty = game_mode_diff;
    proc_set_flag(proc, PROC_FLAG_READY);
    return true;
}

static void* basic_thread_func(void* arg) {
    Process* p = (Process*)arg;
    (void)p;
    sleep(1);
    return NULL;
}

static void proc_do_hpc(Process* p) {
    if (!proc_has_flag(p, PROC_FLAG_HPC) || p->hpc_work <= 0) return;
    long N = p->hpc_work;
    int threads = (p->priority < 1) ? 1 : p->priority;
    HPCPartialArg* arr = (HPCPartialArg*)calloc(threads, sizeof(HPCPartialArg));
    pthread_t* tids = (pthread_t*)calloc(threads, sizeof(pthread_t));
    if (!arr || !tids) {
        p->flags |= PROC_FLAG_ERROR;
        free(arr);
        free(tids);
        return;
    }
    long chunk = N / threads;
    long start = 1;
    for (int i = 0; i < threads; i++) {
        arr[i].start = start;
        if (i == threads - 1) arr[i].end = N + 1;
        else arr[i].end = start + chunk;
        start = arr[i].end;
        pthread_create(&tids[i], NULL, proc_hpc_partial_sum, &arr[i]);
    }
    long total = 0;
    for (int i = 0; i < threads; i++) {
        void* retp = NULL;
        pthread_join(tids[i], &retp);
        HPCPartialArg* rr = (HPCPartialArg*)retp;
        total += rr->partial;
    }
    p->partial_result = total;
    free(arr);
    free(tids);
}

static void proc_do_container(Process* p) {
    if (!proc_has_flag(p, PROC_FLAG_CONTAINER)) return;
    if (!create_ephemeral_dir(p->container_dir, sizeof(p->container_dir))) {
        p->flags |= PROC_FLAG_ERROR;
    }
}

static void proc_do_pipeline(const Process* p) {
    if (!proc_has_flag(p, PROC_FLAG_PIPELINE)) return;
    const int st = (p->pipeline_stages < 1) ? 1 : p->pipeline_stages;
    for (int i = 0; i < st; i++) {
        sleep(1);
    }
}

static void proc_do_distributed(Process* p) {
    if (!proc_has_flag(p, PROC_FLAG_DISTRIBUTED)) return;
    for (int i = 0; i < p->distributed_count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            p->flags |= PROC_FLAG_ERROR;
            continue;
        } else if (pid == 0) {
            sleep(1);
            _exit(0);
        } else {
            waitpid(pid, NULL, 0);
        }
    }
}

bool process_start(Process* proc) {
    if (!proc) return false;
    if (!(proc->flags & PROC_FLAG_READY)) return false;
    proc->flags &= ~PROC_FLAG_READY;
    proc->flags |= PROC_FLAG_RUNNING;

    /* HPC overshadow if HPC is set? We'll do HPC concurrency. */
    if (proc_has_flag(proc, PROC_FLAG_HPC)) {
        proc_do_hpc(proc);
    }
    else if (proc_has_flag(proc, PROC_FLAG_MULTITHREAD)) {
        pthread_create(&proc->tid, NULL, basic_thread_func, proc);
    }

    if (proc_has_flag(proc, PROC_FLAG_CONTAINER)) {
        proc_do_container(proc);
    }
    if (proc_has_flag(proc, PROC_FLAG_PIPELINE)) {
        proc_do_pipeline(proc);
    }
    if (proc_has_flag(proc, PROC_FLAG_DISTRIBUTED)) {
        proc_do_distributed(proc);
    }
    return !(proc->flags & PROC_FLAG_ERROR);
}

bool process_wait(Process* proc) {
    if (!proc) return false;
    if (!(proc->flags & PROC_FLAG_RUNNING)) return true;
    if (proc->tid) {
        pthread_join(proc->tid, NULL);
        proc->tid = 0;
    }
    proc->flags &= ~PROC_FLAG_RUNNING;
    proc->flags |= PROC_FLAG_TERMINATED;
    proc->completed = true;
    return !(proc->flags & PROC_FLAG_ERROR);
}

bool process_destroy(Process* proc) {
    if (!proc) return false;
    if (proc_has_flag(proc, PROC_FLAG_CONTAINER)) {
        if (strstr(proc->container_dir, "/tmp/process_container_")) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "rm -rf %s", proc->container_dir);
            system(cmd);
        }
    }
    free(proc);
    return true;
}
