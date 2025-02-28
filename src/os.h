#ifndef OS_H
#define OS_H

#include <stdint.h>
#include "container.h"

/*
  OS-level stubs and abstractions:
   - ephemeral containers
   - HPC overshadow
   - HPC overlay (newly added)
   - pipeline
   - distributed example
   - time logging for user display logs
   - concurrency stop check
*/

/* Initialize OS environment (buffers, times, etc.). */
void os_init(void);

/* Cleanup OS environment (remove ephemeral containers, etc.). */
void os_cleanup(void);

/* Returns real-world time in ms since os_init(). */
uint64_t os_time(void);

void os_log(const char* msg);
/* Ephemeral container operations. */
void os_create_ephemeral_container(void);
void os_remove_ephemeral_container(void);
/* HPC overshadow => spawns multiple (4) CPU-bound threads */
void os_run_hpc_overshadow(void);
/* HPC overlay => spawns fewer (2) CPU-bound threads */
void os_run_hpc_overlay(void);
/* Pipeline example => fork a child, show pipeline start/end with ASCII. */
void os_pipeline_example(void);
/* Distributed example => fork a child that itself runs HPC overshadow. */
void os_run_distributed_example(void);

void orchestrator_run(container_t* containers, int count);
#endif
