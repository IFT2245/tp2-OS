
#include "worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Fast inline for flags */
static inline void w_set_flag(Worker* w, worker_flags_t f) { w->flags |= f; }
static inline void w_clear_flag(Worker* w, worker_flags_t f) { w->flags &= ~f; }
static inline bool w_has_flag(const Worker* w, worker_flags_t f) { return (w->flags & f) != 0; }

/* HPC sub-thread struct */
typedef struct {
    long start;
    long end;
    long partial;
} HPCPartialArg;

/* HPC sub-thread function: partial sum from start..end-1. */
static void* hpc_subthread_func(void* arg) {
    HPCPartialArg* a = (HPCPartialArg*)arg;
    long sumv = 0;
    for (long i = a->start; i < a->end; i++) {
        sumv += i;
    }
    a->partial = sumv;
    return arg;
}

/* Container ephemeral creation */
static bool create_ephemeral_dir(char* path, size_t sz) {
    if (!path || sz < 12) return false;
    if (!strstr(path, "XXXXXX")) {
        size_t len = strlen(path);
        if (len + 6 < sz) {
            strcat(path, "XXXXXX");
        } else {
            snprintf(path, sz, "/tmp/worker_container_XXXXXX");
        }
    }
    return (mkdtemp(path) != NULL);
}

/* HPC concurrency integrated with pipeline.
   If HPC overshadow is set, each pipeline stage might do partial HPC concurrency. */
static void do_pipeline_with_hpc(Worker* w) {
    int st = (w->pipeline_stages < 1) ? 1 : w->pipeline_stages;
    long chunkBase = (w->hpc_total_work <= 0) ? 100 : w->hpc_total_work;
    for (int i = 0; i < st; i++) {
        long stageN = chunkBase / st;
        HPCPartialArg* arr = (HPCPartialArg*)calloc((size_t)w->hpc_thread_count, sizeof(HPCPartialArg));
        pthread_t* tids = (pthread_t*)calloc((size_t)w->hpc_thread_count, sizeof(pthread_t));
        if (arr && tids) {
            long piece = stageN / w->hpc_thread_count;
            long start = (long)i * stageN + 1;
            for (int t = 0; t < w->hpc_thread_count; t++) {
                arr[t].start = start;
                if (t == w->hpc_thread_count - 1) arr[t].end = start + piece + (stageN % w->hpc_thread_count);
                else arr[t].end = start + piece;
                start = arr[t].end;
                pthread_create(&tids[t], NULL, hpc_subthread_func, &arr[t]);
            }
            long stSum = 0;
            for (int t = 0; t < w->hpc_thread_count; t++) {
                void* retp = NULL;
                pthread_join(tids[t], &retp);
                HPCPartialArg* r = (HPCPartialArg*)retp;
                stSum += r->partial;
            }
            w->hpc_result += stSum;
        } else {
            w_set_flag(w, WORKER_FLAG_ERROR);
        }
        free(arr);
        free(tids);
        w->current_stage = i+1;
        sleep(1);
    }
}

/* Main worker thread that integrates HPC partial sums, ephemeral container, pipeline synergy, distributed processes. */
static void* main_worker_func(void* arg) {
    Worker* w = (Worker*)arg;
    w->active = true;
    w->done = false;

    /* HPC overshadow: if HPC is set,
       and pipeline is set => do advanced HPC for each pipeline stage.
       else do single HPC pass. */
    if (w_has_flag(w, WORKER_FLAG_HPC)) {
        if (w_has_flag(w, WORKER_FLAG_PIPELINE)) {
            do_pipeline_with_hpc(w);
        } else {
            long N = (w->hpc_total_work <= 0) ? 100 : w->hpc_total_work;
            int threads = (w->hpc_thread_count < 1) ? 1 : w->hpc_thread_count;
            HPCPartialArg* arr = (HPCPartialArg*)calloc((size_t)threads, sizeof(HPCPartialArg));
            pthread_t* tids = (pthread_t*)calloc((size_t)threads, sizeof(pthread_t));
            if (arr && tids) {
                long chunk = N / threads;
                long start = 1;
                for (int i = 0; i < threads; i++) {
                    arr[i].start = start;
                    arr[i].end   = (i == threads-1) ? (N+1) : (start + chunk);
                    start = arr[i].end;
                    pthread_create(&tids[i], NULL, hpc_subthread_func, &arr[i]);
                }
                long total = 0;
                for (int i = 0; i < threads; i++) {
                    void* retp = NULL;
                    pthread_join(tids[i], &retp);
                    HPCPartialArg* r = (HPCPartialArg*)retp;
                    total += r->partial;
                }
                w->hpc_result = total;
            } else {
                w_set_flag(w, WORKER_FLAG_ERROR);
            }
            free(arr);
            free(tids);
            if (w_has_flag(w, WORKER_FLAG_PIPELINE)) {
                int st = (w->pipeline_stages < 1) ? 1 : w->pipeline_stages;
                for (int i = 0; i < st; i++) {
                    w->current_stage = i+1;
                    sleep(1);
                }
            }
        }
    }
    else {
        /* HPC not set => do pipeline if flagged. */
        if (w_has_flag(w, WORKER_FLAG_PIPELINE)) {
            int st = (w->pipeline_stages < 1) ? 1 : w->pipeline_stages;
            for (int i = 0; i < st; i++) {
                w->current_stage = i+1;
                sleep(1);
            }
        }
    }

    /* Container ephemeral dir. */
    if (w_has_flag(w, WORKER_FLAG_CONTAINER)) {
        if (!create_ephemeral_dir(w->container_dir, sizeof(w->container_dir))) {
            w_set_flag(w, WORKER_FLAG_ERROR);
        } else {
            w->container_ready = true;
        }
    }

    /* Distributed child processes. */
    if (w_has_flag(w, WORKER_FLAG_DISTRIB)) {
        int dist = (w->distribute_count < 1) ? 1 : w->distribute_count;
        for (int i = 0; i < dist; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                w_set_flag(w, WORKER_FLAG_ERROR);
            } else if (pid == 0) {
                sleep(1);
                _exit(0);
            } else {
                waitpid(pid, NULL, 0);
            }
        }
    }

    /* Debug mode logs. */
    if (w_has_flag(w, WORKER_FLAG_DEBUG)) {
        fprintf(stdout, "[Worker Debug] HPC=%s HPCres=%ld Container=%s PipelineStages=%d DistCount=%d stageNow=%d gameDiff=%d gameScore=%d\n",
                w_has_flag(w, WORKER_FLAG_HPC)?"ON":"OFF", w->hpc_result,
                w->container_ready? w->container_dir : "NO",
                w->pipeline_stages, w->distribute_count,
                w->current_stage, w->game_diff, w->game_score);
        fflush(stdout);
    }

    w->active = false;
    w->done = true;
    return NULL;
}

/* Worker basic API */

Worker* worker_create(void) {
    Worker* w = (Worker*)calloc(1, sizeof(Worker));
    if (!w) return NULL;
    w->flags = WORKER_FLAG_NEW | WORKER_FLAG_COMPAT;
    w->hpc_thread_count = 2;
    w->hpc_total_work   = 100;
    w->hpc_result       = 0;
    w->hpc_threads      = NULL;
    snprintf(w->container_dir, sizeof(w->container_dir), "/tmp/worker_container_XXXXXX");
    w->container_ready  = false;
    w->pipeline_stages  = 3;
    w->current_stage    = 0;
    w->distribute_count = 2;
    w->main_tid         = 0;
    w->active           = false;
    w->done             = false;
    w->game_diff        = 0;
    w->game_score       = 0;
    return w;
}

bool worker_destroy(Worker* w) {
    if (!w) return false;
    if (w_has_flag(w, WORKER_FLAG_CONTAINER) && w->container_ready) {
        if (strstr(w->container_dir, "/tmp/worker_container_")) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "rm -rf %s", w->container_dir);
            system(cmd);
        }
    }
    free(w);
    return true;
}

bool worker_configure(Worker* w, worker_flags_t flags,
                      int hpc_threads, long total_work,
                      const char* container_path,
                      int pipeline_stages,
                      int distribute_count,
                      int game_diff,
                      int game_score) {
    if (!w) return false;
    w->flags |= flags;
    if (hpc_threads > 0)     w->hpc_thread_count = hpc_threads;
    if (total_work  > 0)     w->hpc_total_work   = total_work;
    if (container_path && *container_path) {
        snprintf(w->container_dir, sizeof(w->container_dir), "%s", container_path);
    }
    if (pipeline_stages > 0) w->pipeline_stages = pipeline_stages;
    if (distribute_count >= 0) w->distribute_count = distribute_count;
    w->game_diff  = game_diff;
    w->game_score = game_score;
    w_clear_flag(w, WORKER_FLAG_NEW);
    return true;
}

bool worker_start(Worker* w) {
    if (!w) return false;
    if (w_has_flag(w, WORKER_FLAG_RUNNING)) return true;
    w_clear_flag(w, WORKER_FLAG_DONE);
    w_clear_flag(w, WORKER_FLAG_ERROR);
    w_set_flag(w, WORKER_FLAG_RUNNING);

    if (pthread_create(&w->main_tid, NULL, main_worker_func, w) != 0) {
        w_set_flag(w, WORKER_FLAG_ERROR);
        return false;
    }
    return true;
}

bool worker_join(Worker* w) {
    if (!w) return false;
    if (w->main_tid) {
        pthread_join(w->main_tid, NULL);
        w->main_tid = 0;
    }
    return !w_has_flag(w, WORKER_FLAG_ERROR);
}

bool worker_cancel(Worker* w) {
    if (!w) return false;
    if (w->main_tid) {
        if (pthread_cancel(w->main_tid) == 0) {
            pthread_join(w->main_tid, NULL);
            w->main_tid = 0;
        } else {
            w_set_flag(w, WORKER_FLAG_ERROR);
            return false;
        }
    }
    return true;
}
