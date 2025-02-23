#include "worker.h"
#include <stdio.h>
#include <unistd.h>

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_BLUE    "\033[94m"
#define CLR_GREEN   "\033[92m"

void simulate_process_partial(process_t* p, unsigned long slice_ms, int core_id) {
    if (!p || !slice_ms) return;
    printf(CLR_BLUE "[Worker] Core=%d => Partial run => priority=%d, slice=%lu ms\n" CLR_RESET,
           core_id, p->priority, slice_ms);
    /* Real sleep for user to see concurrency. */
    usleep(slice_ms * 220000); /* slowed for more visual switching: 1ms sim => 220ms real */
    /* This multiplier is intentionally large so you can watch the schedule unfold. */
}
