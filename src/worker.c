#include "worker.h"
#include "stats.h"
#include <stdio.h>
#include <unistd.h>

#define CLR_RESET "\033[0m"
#define CLR_BOLD  "\033[1m"
#define CLR_BLUE  "\033[94m"

static void sim_sleep(unsigned int us){
    int sm = stats_get_speed_mode();
    if(sm==1){
        usleep(us/10 + 1);
    } else {
        usleep(us);
    }
}

void simulate_process_partial(process_t* p, unsigned long slice_ms, int core_id) {
    if(!p || slice_ms==0) return;
    printf(CLR_BLUE "[Worker] Core=%d => Partial run => priority=%d, slice=%lu ms\n" CLR_RESET,
           core_id, p->priority, slice_ms);

    unsigned int real_us = (unsigned int)(slice_ms * 220000);
    sim_sleep(real_us);
}
