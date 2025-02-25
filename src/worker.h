#ifndef WORKER_H
#define WORKER_H

#include "process.h"

/*
  Worker simulation => runs a process for some (partial) timeslice
  by sleeping that many ms in real-time (scaled for concurrency).
*/

/* Simulate a partial run of 'p' for slice_ms. */
void simulate_process_partial(process_t* p, unsigned long slice_ms, int core_id);

#endif
