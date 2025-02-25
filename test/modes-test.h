#ifndef MODES_TEST_H
#define MODES_TEST_H

/*
  Modes test: HPC overshadow, BFS, MLFQ, pipeline, containers, etc.
*/

void run_modes_tests(int* total,int* passed);

/* For single-test picking. */
int         modes_test_get_count(void);
const char* modes_test_get_name(int index);
int         modes_test_run_single(int index);

#endif
