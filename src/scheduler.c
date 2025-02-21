#include "scheduler.h"
#include "ready_queue.h"
#include "worker.h"
#include "os.h"

static scheduler_alg_t current_alg=ALG_CFS;

void scheduler_select_algorithm(scheduler_alg_t alg) {
    current_alg=alg;
}

void scheduler_run(process_t* list,int count) {
    if(!list||count<=0) return;
    if(current_alg==ALG_HPC_OVERSHADOW) {
        os_run_hpc_overshadow();
        return;
    }
    ready_queue_init_policy(current_alg);
    for(int i=0;i<count;i++){
        ready_queue_push(&list[i]);
    }
    while(ready_queue_size()>0){
        process_t* p=ready_queue_pop();
        if(!p) break;
        simulate_process(p);
    }
    ready_queue_destroy();
}
