#ifndef WORKER_H
#define WORKER_H

#include "process.h"

/*
  Worker simulation => actually "runs" a process for some (partial) timeslice
  by sleeping that many milliseconds in real time, purely for user-friendly
  concurrency demonstration. Stats are updated in scheduler code.

  We'll also print an ASCII line describing what's happening for each partial slice.
*/

#ifdef __cplusplus
extern "C" {
#endif

  void simulate_process_partial(process_t* p, unsigned long slice_ms, int core_id);

#ifdef __cplusplus
}
#endif

#endif
