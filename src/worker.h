#ifndef WORKER_H
#define WORKER_H

#include "process.h"

/*
  Functions that "simulate" the process running, using usleep to show
  the user some concurrency. Stats are updated in the scheduler's sim_time domain.
*/
void simulate_process(process_t* p);
void simulate_process_partial(process_t* p, unsigned long slice_ms);

#endif
