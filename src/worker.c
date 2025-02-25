#include "worker.h"
#include "stats.h"
#include <stdio.h>
#include <unistd.h>

/*
  We simulate partial CPU usage by sleeping. If FAST mode => we drastically reduce.
*/
static void sim_sleep(unsigned int us) {
    int sm = stats_get_speed_mode();
    if(sm==1) {
        /* fast => ~1/10 the sleeping or even less. */
        usleep(us / 10 + 1);
    } else {
        usleep(us);
    }
}

void simulate_process_partial(process_t* p, unsigned long slice_ms, int core_id) {
    if(!p || slice_ms==0) return;
    if(stats_get_speed_mode()==0) {
        printf("\033[94m[Worker] Core=%d => Partial run => priority=%d, slice=%lu ms\n\033[0m",
               core_id, p->priority, slice_ms);
    }
    unsigned int real_us = (unsigned int)(slice_ms * 220000);
    sim_sleep(real_us);
}
