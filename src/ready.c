#include "ready.h"
#include "safe_calls_library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static inline void rq_set_flag(ReadyQueue* r, ready_flags_t f) { r->flags |= f; }
static inline void rq_clear_flag(ReadyQueue* r, ready_flags_t f) { r->flags &= ~f; }
static inline bool rq_has_flag(const ReadyQueue* r, ready_flags_t f) { return (r->flags & f) != 0; }

static bool ensure_capacity(ReadyQueue* rq) {
    if (rq->size < rq->capacity) return true;
    int newcap = (rq->capacity == 0) ? 8 : rq->capacity * 2;
    Process** newarr = (Process**)realloc(rq->procs, newcap * sizeof(Process*));
    if (!newarr) {
        rq_set_flag(rq, RQ_FLAG_ERROR);
        return false;
    }
    rq->procs = newarr;
    rq->capacity = newcap;
    return true;
}

ReadyQueue* ready_create(int capacity_hint) {
    if (capacity_hint <= 0) capacity_hint = 8;
    ReadyQueue* rq = (ReadyQueue*)calloc(1, sizeof(ReadyQueue));
    if (!rq) return NULL;
    rq->flags = RQ_FLAG_COMPAT;
    rq->capacity = capacity_hint;
    rq->size = 0;
    rq->procs = (Process**)calloc((size_t)capacity_hint, sizeof(Process*));
    if (!rq->procs) {
        free(rq);
        return NULL;
    }
    rq->sched_quantum_ms = 100;
    rq->default_hpc_threads = 2;
    rq->default_distributed_count = 2;
    rq->default_pipeline_stages = 3;
    rq->game_score = 0;
    rq->game_difficulty = 0;
    return rq;
}

bool ready_configure(ReadyQueue* rq) {
    if (!rq) return false;
    printf("\n-- ReadyQueue Configuration --\n");
    printf("[1] FIFO\n[2] RR\n[3] SJF\n[4] BFS\n[5] Priority\nChoice: ");
    fflush(stdout);

    char buf[64];
    if (!fgets(buf, sizeof(buf), stdin)) {
        rq_set_flag(rq, RQ_FLAG_ERROR);
        return true;
    }
    int choice = parse_int_strtol(buf, 1);
    switch (choice) {
        case 1: rq_set_flag(rq, RQ_SCHED_FIFO);      break;
        case 2: rq_set_flag(rq, RQ_SCHED_RR);        break;
        case 3: rq_set_flag(rq, RQ_SCHED_SJF);       break;
        case 4: rq_set_flag(rq, RQ_SCHED_BFS);       break;
        case 5: rq_set_flag(rq, RQ_SCHED_PRIORITY);  break;
        default: rq_set_flag(rq, RQ_SCHED_FIFO);     break;
    }

    printf("Quantum ms (if RR): ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        int val = parse_int_strtol(buf, rq->sched_quantum_ms);
        if (val > 0) rq->sched_quantum_ms = val;
    }

    printf("Enable HPC? [Y/N]: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (buf[0] == 'Y' || buf[0] == 'y')) {
        rq_set_flag(rq, RQ_MODE_HPC);
        printf("HPC threads (default=2): ");
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin)) {
            int v = parse_int_strtol(buf, rq->default_hpc_threads);
            if (v > 0) rq->default_hpc_threads = v;
        }
    }

    printf("Enable container? [Y/N]: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (buf[0] == 'Y' || buf[0] == 'y')) {
        rq_set_flag(rq, RQ_MODE_CONTAINER);
    }

    printf("Enable pipeline? [Y/N]: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (buf[0] == 'Y' || buf[0] == 'y')) {
        rq_set_flag(rq, RQ_MODE_PIPELINE);
        printf("Pipeline stages (default=3): ");
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin)) {
            int st = parse_int_strtol(buf, rq->default_pipeline_stages);
            if (st > 0) rq->default_pipeline_stages = st;
        }
    }

    printf("Enable distributed? [Y/N]: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (buf[0] == 'Y' || buf[0] == 'y')) {
        rq_set_flag(rq, RQ_MODE_DISTRIBUTED);
        printf("Distributed count (default=2): ");
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin)) {
            int dist = parse_int_strtol(buf, rq->default_distributed_count);
            if (dist >= 0) rq->default_distributed_count = dist;
        }
    }

    printf("Enable debug? [Y/N]: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && (buf[0] == 'Y' || buf[0] == 'y')) {
        rq_set_flag(rq, RQ_MODE_DEBUG);
    }

    return true;
}

bool ready_enqueue(ReadyQueue* rq, Process* proc) {
    if (!rq || !proc) return false;
    if (!ensure_capacity(rq)) return false;
    rq->procs[rq->size++] = proc;
    return true;
}

Process* ready_dequeue(ReadyQueue* rq) {
    if (!rq || rq->size == 0) return NULL;
    /* For FIFO => front item
       For RR => also front, re-enqueue if not done
       For SJF => pick the process with smallest hpc_work? or partial...
       For BFS => same as FIFO
       For Priority => pick highest priority
       We'll do a simplified approach for demonstration: pick front if not flagged otherwise.
       This is minimal but can expand if needed.
    */
    if (rq_has_flag(rq, RQ_SCHED_FIFO) || rq_has_flag(rq, RQ_SCHED_BFS) || rq_has_flag(rq, RQ_SCHED_RR)) {
        Process* ret = rq->procs[0];
        memmove(rq->procs, rq->procs+1, sizeof(Process*)*(rq->size-1));
        rq->size--;
        return ret;
    }
    else if (rq_has_flag(rq, RQ_SCHED_SJF)) {
        int best_idx = 0;
        long best_work = rq->procs[0]->hpc_work;
        for (int i = 1; i < rq->size; i++) {
            if (rq->procs[i]->hpc_work < best_work) {
                best_work = rq->procs[i]->hpc_work;
                best_idx = i;
            }
        }
        Process* ret = rq->procs[best_idx];
        for (int j = best_idx; j < rq->size - 1; j++) {
            rq->procs[j] = rq->procs[j+1];
        }
        rq->size--;
        return ret;
    }
    else if (rq_has_flag(rq, RQ_SCHED_PRIORITY)) {
        int best_idx = 0;
        int best_pri = rq->procs[0]->priority;
        for (int i = 1; i < rq->size; i++) {
            if (rq->procs[i]->priority > best_pri) {
                best_pri = rq->procs[i]->priority;
                best_idx = i;
            }
        }
        Process* ret = rq->procs[best_idx];
        for (int j = best_idx; j < rq->size - 1; j++) {
            rq->procs[j] = rq->procs[j+1];
        }
        rq->size--;
        return ret;
    }
    /* fallback => front item */
    Process* ret = rq->procs[0];
    memmove(rq->procs, rq->procs+1, sizeof(Process*)*(rq->size-1));
    rq->size--;
    return ret;
}

static void apply_queue_flags(const ReadyQueue* rq, Process* p) {
    if (rq_has_flag(rq, RQ_MODE_HPC)) p->flags |= PROC_FLAG_HPC;
    if (rq_has_flag(rq, RQ_MODE_CONTAINER)) p->flags |= PROC_FLAG_CONTAINER;
    if (rq_has_flag(rq, RQ_MODE_PIPELINE)) p->flags |= PROC_FLAG_PIPELINE;
    if (rq_has_flag(rq, RQ_MODE_DISTRIBUTED)) p->flags |= PROC_FLAG_DISTRIBUTED;
    if (rq_has_flag(rq, RQ_MODE_DEBUG)) p->flags |= PROC_FLAG_DEBUG;
    if (rq_has_flag(rq, RQ_SCHED_FIFO)) p->flags |= PROC_FLAG_SCHED_FIFO;
    if (rq_has_flag(rq, RQ_SCHED_RR)) p->flags |= PROC_FLAG_SCHED_RR;
    if (rq_has_flag(rq, RQ_SCHED_SJF)) p->flags |= PROC_FLAG_SCHED_SJF;
    if (rq_has_flag(rq, RQ_SCHED_BFS)) p->flags |= PROC_FLAG_SCHED_BFS;
    if (rq_has_flag(rq, RQ_SCHED_PRIORITY)) p->flags |= PROC_FLAG_SCHED_PRIO;
    p->hpc_work = (long)rq->default_hpc_threads * 100L; /* simplistic approach */
    p->distributed_count = rq->default_distributed_count;
    p->pipeline_stages = rq->default_pipeline_stages;
}

bool ready_run(ReadyQueue* rq) {
    if (!rq) return false;
    while (rq->size > 0 && !rq_has_flag(rq, RQ_FLAG_ERROR)) {
        Process* p = ready_dequeue(rq);
        if (!p) continue;
        apply_queue_flags(rq, p);
        bool started = process_start(p);
        bool waited = process_wait(p);
        (void)started; (void)waited;
        if (p->flags & PROC_FLAG_ERROR) {
            rq_set_flag(rq, RQ_FLAG_ERROR);
        }
        process_destroy(p);
    }
    return !rq_has_flag(rq, RQ_FLAG_ERROR);
}

bool ready_destroy(ReadyQueue* rq) {
    if (!rq) return false;
    for (int i = 0; i < rq->size; i++) {
        if (rq->procs[i]) {
            process_destroy(rq->procs[i]);
        }
    }
    free(rq->procs);
    free(rq);
    return true;
}

void ready_ui_interact(ReadyQueue* rq) {
    if (!rq) return;
    while (true) {
        printf("\n-- Ready Queue UI --\n");
        printf("[1] Scheduling (1=FIFO,2=RR,3=SJF,4=BFS,5=PRIO)\n");
        printf("[2] HPC mode (cur=%s)\n", rq_has_flag(rq, RQ_MODE_HPC)?"ON":"OFF");
        printf("[3] Container mode (cur=%s)\n", rq_has_flag(rq, RQ_MODE_CONTAINER)?"ON":"OFF");
        printf("[4] Pipeline mode (cur=%s)\n", rq_has_flag(rq, RQ_MODE_PIPELINE)?"ON":"OFF");
        printf("[5] Distributed mode (cur=%s)\n", rq_has_flag(rq, RQ_MODE_DISTRIBUTED)?"ON":"OFF");
        printf("[6] Debug mode (cur=%s)\n", rq_has_flag(rq, RQ_MODE_DEBUG)?"ON":"OFF");
        printf("[7] HPC default threads (%d)\n", rq->default_hpc_threads);
        printf("[8] Dist default count (%d)\n", rq->default_distributed_count);
        printf("[9] Pipeline default stages (%d)\n", rq->default_pipeline_stages);
        printf("[Q] quantum ms (%d)\n", rq->sched_quantum_ms);
        printf("[E] Enqueue sample process\n");
        printf("[R] Run queue\n");
        printf("[0] Quit UI\n");
        printf("Choice: ");
        fflush(stdout);

        char buf[64];
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("End input.\n");
            break;
        }
        if (buf[0] == '0') {
            break;
        }
        switch(buf[0]) {
            case '1': {
                printf("Pick sched: (1=FIFO,2=RR,3=SJF,4=BFS,5=PRIO): ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                int c = parse_int_strtol(buf, 1);
                rq->flags &= ~(RQ_SCHED_FIFO|RQ_SCHED_RR|RQ_SCHED_SJF|RQ_SCHED_BFS|RQ_SCHED_PRIORITY);
                switch(c) {
                    case 1: rq->flags |= RQ_SCHED_FIFO; break;
                    case 2: rq->flags |= RQ_SCHED_RR;   break;
                    case 3: rq->flags |= RQ_SCHED_SJF;  break;
                    case 4: rq->flags |= RQ_SCHED_BFS;  break;
                    case 5: rq->flags |= RQ_SCHED_PRIORITY; break;
                    default: rq->flags |= RQ_SCHED_FIFO; break;
                }
                break;
            }
            case '2': {
                if (rq_has_flag(rq, RQ_MODE_HPC)) rq->flags &= ~RQ_MODE_HPC;
                else rq->flags |= RQ_MODE_HPC;
                break;
            }
            case '3': {
                if (rq_has_flag(rq, RQ_MODE_CONTAINER)) rq->flags &= ~RQ_MODE_CONTAINER;
                else rq->flags |= RQ_MODE_CONTAINER;
                break;
            }
            case '4': {
                if (rq_has_flag(rq, RQ_MODE_PIPELINE)) rq->flags &= ~RQ_MODE_PIPELINE;
                else rq->flags |= RQ_MODE_PIPELINE;
                break;
            }
            case '5': {
                if (rq_has_flag(rq, RQ_MODE_DISTRIBUTED)) rq->flags &= ~RQ_MODE_DISTRIBUTED;
                else rq->flags |= RQ_MODE_DISTRIBUTED;
                break;
            }
            case '6': {
                if (rq_has_flag(rq, RQ_MODE_DEBUG)) rq->flags &= ~RQ_MODE_DEBUG;
                else rq->flags |= RQ_MODE_DEBUG;
                break;
            }
            case '7': {
                printf("New HPC threads: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                int val = parse_int_strtol(buf, rq->default_hpc_threads);
                if (val > 0) rq->default_hpc_threads = val;
                break;
            }
            case '8': {
                printf("New distributed count: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                int val = parse_int_strtol(buf, rq->default_distributed_count);
                if (val >= 0) rq->default_distributed_count = val;
                break;
            }
            case '9': {
                printf("New pipeline stages: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                int val = parse_int_strtol(buf, rq->default_pipeline_stages);
                if (val > 0) rq->default_pipeline_stages = val;
                break;
            }
            case 'Q':
            case 'q': {
                printf("New quantum ms: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                int val = parse_int_strtol(buf, rq->sched_quantum_ms);
                if (val > 0) rq->sched_quantum_ms = val;
                break;
            }
            case 'E':
            case 'e': {
                Process* p = process_create();
                if (!p) {
                    printf("Failed to create new process.\n");
                    break;
                }
                ready_enqueue(rq, p);
                printf("Enqueued a sample process with HPCwork=100, etc.\n");
                break;
            }
            case 'R':
            case 'r': {
                bool ok = ready_run(rq);
                printf("Queue run => %s\n", ok?"Success":"Error");
                break;
            }
            default:
                printf("Invalid.\n");
                break;
        }
    }
}
