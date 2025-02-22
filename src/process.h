#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>

typedef struct process_s {
    uint64_t burst_time;
    int priority;
    uint64_t vruntime;
    uint64_t arrival_time;
    uint64_t remaining_time;
    uint64_t last_exec;
    int mlfq_level;
} process_t;

void init_process(process_t* p, uint64_t burst, int priority, uint64_t arrival);

#endif
