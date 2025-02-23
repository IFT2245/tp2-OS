#ifndef OS_H
#define OS_H

#include <stdint.h>

/*
  OS-level stubs and abstractions:
   - ephemeral containers
   - HPC overshadow
   - pipeline
   - distributed example
   - time logging for user feedback
*/

void     os_init(void);
void     os_cleanup(void);

/* Returns the real-world time (ms) since os_init(), purely for user display. */
uint64_t os_time(void);

/* Optionally log a message with a short delay for user-friendly pacing. */
void     os_log(const char* msg);

/* Ephemeral container operations. */
void     os_create_ephemeral_container(void);
void     os_remove_ephemeral_container(void);

/* HPC overshadow => spawns multiple CPU-bound threads to demonstrate concurrency. */
void     os_run_hpc_overshadow(void);

/* Pipeline example => fork a child, show pipeline start and end with ASCII. */
void     os_pipeline_example(void);

/* Distributed example => fork a child that itself runs HPC overshadow. */
void     os_run_distributed_example(void);


#endif
