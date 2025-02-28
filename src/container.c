#include "ready_queue.h"
#include "worker.h"
#include "../lib/log.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

/* =========== (A) ephemeral remove code =========== */
#ifdef EPHEMERAL_RM_RECURSIVE
static int remove_directory_recursive(const char* path){
    DIR* dir = opendir(path);
    if(!dir) return rmdir(path);
    struct dirent* entry;
    int ret=0;
    while((entry = readdir(dir))){
        if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0){
            continue;
        }
        char buf[512];
        snprintf(buf,sizeof(buf),"%s/%s", path, entry->d_name);
        struct stat st;
        if(stat(buf,&st)==0){
            if(S_ISDIR(st.st_mode)){
                ret=remove_directory_recursive(buf);
                if(ret!=0) break;
            } else {
                ret=unlink(buf);
                if(ret!=0) break;
            }
        }
    }
    closedir(dir);
    if(ret==0) ret=rmdir(path);
    return ret;
}
#endif

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
    /* Make ephemeral container logs purple: */
    log_info("\033[35mephemeral created => %s\033[0m", p);
    return p;
}

static void ephemeral_remove_container(const char* path){
    if(!path) return;
#ifdef EPHEMERAL_RM_RECURSIVE
    int r = remove_directory_recursive(path);
#else
    int r = rmdir(path);
#endif
    if(r==0){
        log_info("\033[35mephemeral removed => %s\033[0m", path);
    } else {
        log_warn("ephemeral remove fail => %s : %s", path, strerror(errno));
    }
}

/* =========== (B) timeline printing =========== */
static int compare_timeline(const void* a, const void* b){
    const timeline_item_t* A = (const timeline_item_t*)a;
    const timeline_item_t* B = (const timeline_item_t*)b;
    if(A->core_id < B->core_id) return -1;
    if(A->core_id > B->core_id) return 1;
    if(A->start_ms < B->start_ms) return -1;
    if(A->start_ms > B->start_ms) return 1;
    return 0;
}

static void print_container_timeline(const container_t* c){
    if(c->timeline_count == 0){
        printf("\n\033[1m\033[33mNo timeline for container.\033[0m\n");
        return;
    }
    qsort(c->timeline, c->timeline_count, sizeof(c->timeline[0]), compare_timeline);

    printf("\033[1m\033[36m\n--- Container Timeline ---\n\033[0m");
    int current_core=9999999;
    for(int i=0; i<c->timeline_count; i++){
        int cid = c->timeline[i].core_id;
        if(cid != current_core){
            if(cid >= 0){
                printf("\033[1m\033[32m\nMain Core %d:\n\033[0m", cid);
            } else {
                int hpc_idx = (-1 - cid);
                printf("\033[1m\033[35m\nHPC Thread %d:\n\033[0m", hpc_idx);
            }
            current_core=cid;
        }
        unsigned long st=c->timeline[i].start_ms;
        unsigned long ln=c->timeline[i].length_ms;
        bool pre=c->timeline[i].preempted_slice;
        if(pre){
            printf("  T[%lu..%lu] => P%d \033[1m\033[33m[PREEMPT]\033[0m\n",
                   st, st+ln, c->timeline[i].proc_id);
        } else {
            printf("  T[%lu..%lu] => P%d\n", st, st+ln, c->timeline[i].proc_id);
        }
    }
}

/* =========== (C) container_run =========== */
static void* container_thread_runner(void* arg){
    container_t* c=(container_t*)arg;
    if(!c){
        log_error("container_run => null container pointer?");
        return NULL;
    }

    c->ephemeral_path = ephemeral_create_container();
    if(!c->ephemeral_path){
        log_error("container_run => ephemeral creation failed => ignoring");
    }

    /* HPC procs offset 1000. */
    for(int i=0; i<c->main_count; i++){
        c->main_procs[i].id = i;
    }
    for(int i=0; i<c->hpc_count; i++){
        c->hpc_procs[i].id = 1000 + i;
    }

    /* Build local RQs. */
    ready_queue_t main_q, hpc_q;
    rq_init(&main_q, c->main_alg);
    rq_init(&hpc_q, c->hpc_alg);

    /* Push immediate arrivals. */
    for(int i=0; i<c->main_count; i++){
        process_t* p=&c->main_procs[i];
        if(p->remaining_time>0 && p->arrival_time==0){
            rq_push(&main_q, p);
        }
    }
    for(int i=0; i<c->hpc_count; i++){
        process_t* p=&c->hpc_procs[i];
        if(p->remaining_time>0 && p->arrival_time==0){
            rq_push(&hpc_q, p);
        }
    }

    /* Create main threads. */
    pthread_t* main_threads = calloc(c->nb_cores, sizeof(pthread_t));
    if(!main_threads){
        log_error("container_run => no memory for main_threads");
        return NULL;
    }
    for(int i=0; i<c->nb_cores; i++){
        core_thread_pack_t* pack = malloc(sizeof(core_thread_pack_t));
        pack->container = c;
        pack->qs.main_rq = &main_q;
        pack->qs.hpc_rq  = &hpc_q;
        pack->core_id = i;
        pthread_create(&main_threads[i], NULL, main_core_thread, pack);
    }

    /* HPC threads. */
    pthread_t* hpc_threads = NULL;
    if(c->nb_hpc_threads > 0){
        hpc_threads = calloc(c->nb_hpc_threads, sizeof(pthread_t));
        if(!hpc_threads){
            log_error("container_run => no memory for hpc_threads");
        } else {
            for(int i=0; i<c->nb_hpc_threads; i++){
                core_thread_pack_t* pack = malloc(sizeof(core_thread_pack_t));
                pack->container = c;
                pack->qs.main_rq = &main_q;
                pack->qs.hpc_rq  = &hpc_q;
                pack->core_id = i;
                pthread_create(&hpc_threads[i], NULL, hpc_thread, pack);
            }
        }
    }

    /* Join main cores. */
    for(int i=0; i<c->nb_cores; i++){
        pthread_join(main_threads[i], NULL);
    }
    free(main_threads);

    /* Join HPC threads. */
    if(c->nb_hpc_threads>0 && hpc_threads){
        for(int i=0; i<c->nb_hpc_threads; i++){
            pthread_join(hpc_threads[i], NULL);
        }
        free(hpc_threads);
    }

    rq_destroy(&main_q);
    rq_destroy(&hpc_q);

    /* ephemeral remove */
    if(c->ephemeral_path){
        ephemeral_remove_container(c->ephemeral_path);
        free(c->ephemeral_path);
        c->ephemeral_path=NULL;
    }

    print_container_timeline(c);

    if(c->timeline){
        free(c->timeline);
        c->timeline = NULL;
    }
    pthread_mutex_destroy(&c->timeline_lock);
    pthread_mutex_destroy(&c->finish_lock);

    return NULL;
}

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
        log_error("container_init => null container pointer");
        return;
    }
    memset(c,0,sizeof(*c));

    if(nb_cores<0) nb_cores=0;
    if(nb_hpc_threads<0) nb_hpc_threads=0;
    if(max_cpu_ms==0){
        log_warn("container_init => forcing max_cpu_ms=100");
        max_cpu_ms=100;
    }

    c->nb_cores       = nb_cores;
    c->nb_hpc_threads = nb_hpc_threads;
    c->main_alg       = main_alg;
    c->hpc_alg        = hpc_alg;
    c->main_procs     = main_list;
    c->main_count     = main_count;
    c->hpc_procs      = hpc_list;
    c->hpc_count      = hpc_count;
    c->max_cpu_time_ms= max_cpu_ms;
    c->remaining_count= main_count + hpc_count;

    pthread_mutex_init(&c->finish_lock,NULL);
    pthread_mutex_init(&c->timeline_lock,NULL);

    c->timeline       = NULL;
    c->timeline_count = 0;
    c->timeline_cap   = 0;
    c->time_exhausted = false;
    c->accumulated_cpu= 0;
    c->sim_time       = 0;

    /* HPC steal if 0 main cores but have main processes. */
    if(nb_cores == 0 && main_count>0){
        log_info("container_init => no main cores but main processes => enabling HPC steal");
        c->allow_hpc_steal = true;
    }
}

void container_run(container_t* c){
    container_thread_runner((void*)c);
}

void orchestrator_run(container_t* arr, int count){
    pthread_t* tids = calloc(count, sizeof(pthread_t));
    if(!tids){
        log_error("orchestrator_run => cannot allocate");
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
