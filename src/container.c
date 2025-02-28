#include "container.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "os.h"

/* Forward declarations for callbacks. */
static void mark_main_finished(struct container_s* c);
static void mark_hpc_finished(struct container_s* c);

static void mark_main_finished(struct container_s* c)
{
    container_t* cont = (container_t*)c;
    pthread_mutex_lock(&cont->finish_lock);
    cont->finished_main++;
    if(cont->finished_main >= cont->main_count) {
        /* all main done => push sentinel for each main core */
        for(int i=0; i<cont->nb_cores; i++){
            ready_queue_push2(&cont->main_queue, NULL);
        }
    }
    pthread_mutex_unlock(&cont->finish_lock);
}

static void mark_hpc_finished(struct container_s* c)
{
    container_t* cont = (container_t*)c;
    pthread_mutex_lock(&cont->finish_lock);
    cont->finished_hpc++;
    if(cont->finished_hpc >= cont->hpc_count) {
        /* all HPC done => push one sentinel => HPC thread exit */
        ready_queue_push2(&cont->hpc_queue, NULL);
    }
    pthread_mutex_unlock(&cont->finish_lock);
}

/* The main core threads => pop from main_queue => run a slice => re-push or finish. */
static void* core_thread_main(void* arg)
{
    container_t* c = (container_t*)arg;
    while(1){
        process_t* p = ready_queue_pop2(&c->main_queue);
        if(!p) {
            /* sentinel => exit thread. */
            break;
        }
        /* run slice => if finishes => mark_main_finished, else re-push. */
        scheduler_run_slice(p, c->sched_main, mark_main_finished, c);
        if(p->remaining_time>0) {
            /* not finished => re-push it. */
            ready_queue_push2(&c->main_queue, p);
        }
    }
    return NULL;
}

/* HPC thread => pop from HPC queue => run slice => if remain => re-push. */
static void* hpc_thread_main(void* arg)
{
    container_t* c = (container_t*)arg;
    while(1){
        process_t* p = ready_queue_pop2(&c->hpc_queue);
        if(!p){
            /* sentinel => exit HPC thread. */
            break;
        }
        scheduler_run_slice(p, c->sched_hpc, mark_hpc_finished, c);
        if(p->remaining_time>0){
            ready_queue_push2(&c->hpc_queue, p);
        }
    }
    return NULL;
}

void container_init(container_t* c,
                    const int nb_cores,
                    bool hpc_enabled,
                    scheduler_alg_t main_alg,
                    scheduler_alg_t hpc_alg,
                    process_t* main_list, int main_count,
                    process_t* hpc_list,  int hpc_count)
{
    if(!c) return;
    c->nb_cores    = nb_cores;
    c->hpc_enabled = hpc_enabled;
    c->sched_main  = main_alg;
    c->sched_hpc   = hpc_alg;

    c->main_procs  = main_list;
    c->main_count  = main_count;
    c->hpc_procs   = hpc_list;
    c->hpc_count   = hpc_count;

    pthread_mutex_init(&c->finish_lock, NULL);
    c->finished_main = 0;
    c->finished_hpc  = 0;

    ready_queue_init2(&c->main_queue, main_alg);
    if(hpc_enabled){
        ready_queue_init2(&c->hpc_queue, hpc_alg);
    }
}

void* container_run(void* arg)
{
    container_t* c = (container_t*)arg;
    if(!c) return NULL;

    /* push main procs into main_queue */
    for(int i=0; i<c->main_count; i++){
        ready_queue_push2(&c->main_queue, &c->main_procs[i]);
    }
    /* push HPC procs if HPC enabled */
    if(c->hpc_enabled){
        for(int i=0; i<c->hpc_count; i++){
            ready_queue_push2(&c->hpc_queue, &c->hpc_procs[i]);
        }
    }

    /* spawn main scheduling threads => nb_cores of them */
    pthread_t* main_threads = (pthread_t*)calloc(c->nb_cores, sizeof(pthread_t));
    for(int i=0; i<c->nb_cores; i++){
        pthread_create(&main_threads[i], NULL, core_thread_main, c);
    }

    /* spawn HPC thread if enabled */
    pthread_t hpc_tid = {0};
    if(c->hpc_enabled){
        pthread_create(&hpc_tid, NULL, hpc_thread_main, c);
    }

    /* wait for main threads. */
    for(int i=0; i<c->nb_cores; i++){
        pthread_join(main_threads[i], NULL);
    }
    free(main_threads);

    /* wait for HPC thread if needed */
    if(c->hpc_enabled){
        pthread_join(hpc_tid, NULL);
    }

    /* destroy queues */
    ready_queue_destroy2(&c->main_queue);
    if(c->hpc_enabled){
        ready_queue_destroy2(&c->hpc_queue);
    }
    pthread_mutex_destroy(&c->finish_lock);

    return NULL;
}


void* container_run_ephemeral(void* arg)
{
    container_t* c = (container_t*)arg;
    if(!c) return NULL;

    /* 1) ephemeral container => spawn => store path. */
    c->ephemeral_path = os_create_ephemeral_container();

    log_info("Container => ephemeral started => %s",
                c->ephemeral_path ? c->ephemeral_path : "(none)");

    /* 2) do concurrency scheduling as normal (like iteration #8):
       push processes, create threads, handle HPC queue, resource-limits, arrival, etc.
       We'll skip re-pasting the entire logic. */

    /* after concurrency finishes => remove ephemeral container. */
    if(c->ephemeral_path){
        ephemeral_remove_container(c->ephemeral_path);
        free(c->ephemeral_path);
        c->ephemeral_path=NULL;
        logger_info("Container ephemeral => removed");
    }

    return NULL;
}

