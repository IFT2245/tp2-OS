#ifndef EXTERNAL_TEST_H
#define EXTERNAL_TEST_H

/*
  run_external_tests():
    For each scheduling mode, we do a trivial run of HPC overshadow or
    short process, then a concurrency test: 'sleep 2'.
    concurrency timeline is printed by run_shell_commands_concurrently().
*/
void run_external_tests(void);

#endif
