#ifndef OS_H
#define OS_H

#include <stdint.h>

/* Initializes the OS environment (time, containers, etc.) */
void     os_init(void);

/* Cleans up resources allocated by os_init */
void     os_cleanup(void);

/* Returns the time since os_init in ms */
uint64_t os_time(void);

/* Logs a simple message to stdout */
void     os_log(const char* msg);

/* Creates an ephemeral container directory (if possible) */
void     os_create_ephemeral_container(void);

/* Removes last ephemeral container (LIFO) */
void     os_remove_ephemeral_container(void);

/* Simulates HPC overshadow computation (multithreaded) */
void     os_run_hpc_overshadow(void);

/* Example of a simple pipeline with fork + wait */
void     os_pipeline_example(void);

/* Example of a distributed HPC overshadow process via fork + wait */
void     os_run_distributed_example(void);

#endif
