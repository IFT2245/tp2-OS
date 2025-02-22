#ifndef WORKER_H
#define WORKER_H
#include "process.h"

void simulate_process(process_t* p);
void simulate_process_partial(process_t* p,unsigned long slice_ms);

#endif
