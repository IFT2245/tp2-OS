#ifndef OS_H
#define OS_H

#include <stdint.h>

/*
  OS-level stubs and abstractions:
   - ephemeral containers
   - HPC overshadow
   - pipeline
   - distributed example
   - time logging for user display logs
   - concurrency stop check
*/

/* Sets the concurrency stop flag (e.g. after SIGUSR1). */
void set_os_concurrency_stop_flag(int val);

/* Returns if concurrency stop was requested. */
int os_concurrency_stop_requested(void);

/* Initialize OS environment (buffers, times, etc.). */
void os_init(void);

/* Cleanup OS environment (remove ephemeral containers, etc.). */
void os_cleanup(void);

/* Returns real-world time in ms since os_init(). */
uint64_t os_time(void);

/* Optionally log a message with a short delay for user-friendly pacing. */
void os_log(const char* msg);

/* Ephemeral container operations. */
void os_create_ephemeral_container(void);
void os_remove_ephemeral_container(void);

/* HPC overshadow => spawns multiple CPU-bound threads to demonstrate concurrency. */
void os_run_hpc_overshadow(void);

/* Pipeline example => fork a child, show pipeline start/end with ASCII. */
void os_pipeline_example(void);

/* Distributed example => fork a child that itself runs HPC overshadow. */
void os_run_distributed_example(void);

#endif
