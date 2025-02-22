#ifndef WORKER_H
#define WORKER_H

#include "process.h"

/* Simulates full burst of the process (blocking sleep) */
void simulate_process(process_t* p);

/* Simulates partial timeslice of the process (blocking sleep) */
void simulate_process_partial(process_t* p, unsigned long slice_ms);

#endif
