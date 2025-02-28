#include "container.h"
#include "scheduler.h"
#include "ready_queue.h"
#include "worker.h"      // For main_core_thread() and hpc_thread() functions
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>

/* ------------------------------------------------------------------
   (A) ephemeral code inlined into container
   ------------------------------------------------------------------ */

#ifdef EPHEMERAL_RM_RECURSIVE
static int remove_directory_recursive(const char* path){
    DIR* dir = opendir(path);
    if(!dir){
        return rmdir(path);
    }
    struct dirent* entry;
    int ret=0;
    while((entry = readdir(dir))){
        if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0){
            continue;
        }
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);

        struct stat st;
        if(stat(buf, &st)==0){
            if(S_ISDIR(st.st_mode)){
                ret = remove_directory_recursive(buf);
                if(ret!=0) break;
            } else {
                ret = unlink(buf);
                if(ret!=0) break;
            }
        }
    }
    closedir(dir);
    if(ret==0){
        ret = rmdir(path);
    }
    return ret;
}
#endif

void record_timeline(container_t* c,
                     int core_id,
                     int proc_id,
                     unsigned long start_ms,
                     unsigned long slice,
                     bool preempted_flag)
{
    pthread_mutex_lock(&c->timeline_lock);

    // Make sure we have space
    if (c->timeline_count + 1 >= c->timeline_cap) {
        int new_cap = c->timeline_cap + 64;
        void* tmp = realloc(c->timeline, new_cap * sizeof(*c->timeline));
        if (!tmp) {
            log_error("record_timeline => out of memory!");
            pthread_mutex_unlock(&c->timeline_lock);
            return;
        }
        c->timeline = tmp;
        c->timeline_cap = new_cap;
    }

    // Append new item
    c->timeline[c->timeline_count].core_id         = core_id;
    c->timeline[c->timeline_count].proc_id         = proc_id;
    c->timeline[c->timeline_count].start_ms        = start_ms;
    c->timeline[c->timeline_count].length_ms       = slice;
    c->timeline[c->timeline_count].preempted_slice = preempted_flag;

    c->timeline_count++;
    pthread_mutex_unlock(&c->timeline_lock);
}


static char* ephemeral_create_container(void){
    char tmpl[] = "/tmp/container_XXXXXX";
    char* p = (char*)malloc(256);
    if(!p){
        log_error("ephemeral_create_container mem fail");
        return NULL;
    }
    strcpy(p, tmpl);
    if(!mkdtemp(p)){
        log_error("mkdtemp fail %s => %s", p, strerror(errno));
        free(p);
        return NULL;
    }
    log_info("ephemeral created => %s", p);
    return p;
}

static void ephemeral_remove_container(const char* path){
    if(!path) return;
#ifdef EPHEMERAL_RM_RECURSIVE
    int r = remove_directory_recursive(path);
#else
    int r = rmdir(path);
#endif
    if(r == 0){
        log_info("ephemeral removed => %s", path);
    } else {
        log_warn("ephemeral remove fail => %s : %s", path, strerror(errno));
    }
}

/* ------------------------------------------------------------------
   (B) container_run support
   ------------------------------------------------------------------ */

/**
 * @brief Print the timeline after the container finishes.
 */
static int compare_timeline(const void* a, const void* b){
    /* Sort by core_id ascending, then start_ms ascending */
    const timeline_item_t* A = (const timeline_item_t*)a;
    const timeline_item_t* B = (const timeline_item_t*)b;
    if(A->core_id < B->core_id) return -1;
    if(A->core_id > B->core_id) return 1;
    if(A->start_ms < B->start_ms) return -1;
    if(A->start_ms > B->start_ms) return 1;
    return 0;
}

static void print_container_timeline(container_t* c){
    if(c->timeline_count == 0){
        printf("\n\033[1m\033[33mNo timeline for container.\n\033[0m");
        return;
    }
    qsort(c->timeline, c->timeline_count, sizeof(c->timeline[0]), compare_timeline);

    printf("\033[1m\033[36m\n--- Container Timeline ---\n\033[0m");
    int current_core = 999999;
    for(int i=0; i < c->timeline_count; i++){
        int core_id = c->timeline[i].core_id;
        if(core_id != current_core){
            if(core_id >= 0){
                printf("\033[1m\033[32m\nMain Core %d:\n\033[0m", core_id);
            } else {
                int hpc_idx = (-1 - core_id);
                printf("\033[1m\033[35m\nHPC Thread %d:\n\033[0m", hpc_idx);
            }
            current_core = core_id;
        }
        unsigned long st = c->timeline[i].start_ms;
        unsigned long ln = c->timeline[i].length_ms;
        bool pre = c->timeline[i].preempted_slice;
        if(pre){
            printf("  T[%lu..%lu] => P%d \033[1m\033[33m[PREEMPT]\033[0m\n",
                   st, st+ln, c->timeline[i].proc_id);
        } else {
            printf("  T[%lu..%lu] => P%d\n", st, st+ln, c->timeline[i].proc_id);
        }
    }
}

static void* container_thread_runner(void* arg){
    container_t* c = (container_t*)arg;
    if(!c){
        log_error("container_run => null container pointer?");
        return NULL;
    }

    /* ephemeral creation */
    c->ephemeral_path = ephemeral_create_container();
    if(!c->ephemeral_path){
        log_error("container_run => ephemeral creation failed. Will run anyway.");
    }

    /* Give IDs. HPC offset by 1000. */
    for(int i=0;i<c->main_count;i++){
        c->main_procs[i].id = i;
    }
    for(int i=0;i<c->hpc_count;i++){
        c->hpc_procs[i].id = 1000 + i;
    }

    /* Create local queues */
    ready_queue_t main_q, hpc_q;
    rq_init(&main_q, c->main_alg);
    rq_init(&hpc_q,  c->hpc_alg);

    /* Push immediate arrivals (arrival_time=0). */
    for(int i=0; i < c->main_count; i++){
        process_t* p = &c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time==0){
            rq_push(&main_q, p);
        }
    }
    for(int i=0; i < c->hpc_count; i++){
        process_t* p = &c->hpc_procs[i];
        if(p->remaining_time>0 && p->arrival_time==0){
            rq_push(&hpc_q, p);
        }
    }

    /* Start main core threads. */
    pthread_t* main_threads = (pthread_t*)calloc(c->nb_cores, sizeof(pthread_t));
    if(!main_threads){
        log_error("container_run => cannot allocate main_threads");
        return NULL;
    }
    for(int i=0; i < c->nb_cores; i++){
        core_thread_pack_t* pack = (core_thread_pack_t*)malloc(sizeof(core_thread_pack_t));
        pack->container = c;
        pack->qs.main_rq = &main_q;
        pack->qs.hpc_rq  = &hpc_q;
        pack->core_id    = i;
        pthread_create(&main_threads[i], NULL, main_core_thread, pack);
    }

    /* Start HPC threads. */
    pthread_t* hpc_threads = NULL;
    if(c->nb_hpc_threads > 0){
        hpc_threads = (pthread_t*)calloc(c->nb_hpc_threads, sizeof(pthread_t));
        if(!hpc_threads){
            log_error("container_run => cannot allocate hpc_threads");
        } else {
            for(int i=0; i < c->nb_hpc_threads; i++){
                core_thread_pack_t* pack = (core_thread_pack_t*)malloc(sizeof(core_thread_pack_t));
                pack->container = c;
                pack->qs.main_rq = &main_q;
                pack->qs.hpc_rq  = &hpc_q;
                pack->core_id    = i; /* HPC index, not negative here */
                pthread_create(&hpc_threads[i], NULL, hpc_thread, pack);
            }
        }
    }

    /* Join main threads. */
    for(int i=0; i < c->nb_cores; i++){
        pthread_join(main_threads[i], NULL);
    }
    free(main_threads);

    /* Join HPC threads. */
    if(c->nb_hpc_threads > 0 && hpc_threads){
        for(int i=0; i < c->nb_hpc_threads; i++){
            pthread_join(hpc_threads[i], NULL);
        }
        free(hpc_threads);
    }

    /* Destroy queues */
    rq_destroy(&main_q);
    rq_destroy(&hpc_q);

    /* ephemeral remove */
    if(c->ephemeral_path){
        ephemeral_remove_container(c->ephemeral_path);
        free(c->ephemeral_path);
        c->ephemeral_path = NULL;
    }

    /* Print timeline */
    print_container_timeline(c);

    /* Clean up timeline array. */
    if(c->timeline){
        free(c->timeline);
        c->timeline = NULL;
    }
    pthread_mutex_destroy(&c->timeline_lock);
    pthread_mutex_destroy(&c->finish_lock);

    return NULL;
}

/* ------------------------------------------------------------------
   (C) container_init and container_run
   ------------------------------------------------------------------ */

void container_init(container_t* c,
                    int nb_cores,
                    int nb_hpc_threads,
                    scheduler_alg_t main_alg,
                    scheduler_alg_t hpc_alg,
                    process_t* main_list,
                    int main_count,
                    process_t* hpc_list,
                    int hpc_count,
                    unsigned long max_cpu_ms)
{
    if(!c){
        log_error("container_init => container pointer is NULL");
        return;
    }
    memset(c, 0, sizeof(*c));

    if(nb_cores < 0 || nb_hpc_threads < 0){
        log_warn("container_init with negative core/hpc => forcing 0");
        if(nb_cores < 0)        nb_cores = 0;
        if(nb_hpc_threads < 0)  nb_hpc_threads = 0;
    }
    if(max_cpu_ms == 0){
        log_warn("container_init => max_cpu_ms=0 => forcing 100");
        max_cpu_ms = 100;
    }

    c->nb_cores        = nb_cores;
    c->nb_hpc_threads  = nb_hpc_threads;
    c->main_alg        = main_alg;
    c->hpc_alg         = hpc_alg;
    c->main_procs      = main_list;
    c->main_count      = main_count;
    c->hpc_procs       = hpc_list;
    c->hpc_count       = hpc_count;
    c->max_cpu_time_ms = max_cpu_ms;
    c->remaining_count = main_count + hpc_count;

    pthread_mutex_init(&c->finish_lock, NULL);
    pthread_mutex_init(&c->timeline_lock, NULL);

    c->timeline       = NULL;
    c->timeline_count = 0;
    c->timeline_cap   = 0;
    c->time_exhausted = false;
    c->accumulated_cpu= 0;
    c->sim_time       = 0;

    /* If we have 0 main cores but still have main processes,
       let HPC threads steal from main automatically to avoid deadlock. */
    if(nb_cores == 0 && main_count > 0){
        log_info("container_init => no main cores but main processes => enabling HPC steal");
        c->allow_hpc_steal = true;
    } else {
        c->allow_hpc_steal = false;
    }
}

void container_run(container_t* c){
    /* Single-thread approach: we can just call container_thread_runner. */
    container_thread_runner((void*)c);
}

void orchestrator_run(container_t* arr, int count){
    /* If you want each container to run in its own thread, do it here.
       Or run them sequentially.
       We'll do them in parallel below for demonstration. */
    pthread_t* tids = (pthread_t*)calloc(count, sizeof(pthread_t));
    if(!tids){
        log_error("orchestrator_run => cannot allocate thread array");
        return;
    }
    for(int i=0; i<count; i++){
        pthread_create(&tids[i], NULL, container_thread_runner, &arr[i]);
    }
    for(int i=0; i<count; i++){
        pthread_join(tids[i], NULL);
    }
    free(tids);
}
