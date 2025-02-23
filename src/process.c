#include "process.h"
#include <string.h>

void init_process(process_t* p, uint64_t burst, int priority, uint64_t arrival) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->burst_time     = burst;
    p->remaining_time = burst;
    p->priority       = priority;
    p->arrival_time   = arrival;
    p->times_owning_core = 0;
}
