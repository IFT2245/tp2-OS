#ifndef WORKER_H
#define WORKER_H

#include "process.h"

/*
  simulate_process() => "Full run" = sleeps for burst_time (ms).
  simulate_process_partial() => "Partial run" for slice ms.
*/
void simulate_process(process_t* p);
void simulate_process_partial(process_t* p, unsigned long slice_ms);

#endif
