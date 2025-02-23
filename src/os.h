#ifndef OS_H
#define OS_H

#include <stdint.h>

/*
  OS-level stubs:
   - ephemeral containers
   - HPC overshadow
   - pipeline
   - distributed example
   - time logging
*/
void     os_init(void);
void     os_cleanup(void);
uint64_t os_time(void);      /* real-time offset for user display only */
void     os_log(const char* msg);

void     os_create_ephemeral_container(void);
void     os_remove_ephemeral_container(void);

void     os_run_hpc_overshadow(void);
void     os_pipeline_example(void);
void     os_run_distributed_example(void);

#endif
