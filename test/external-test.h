#ifndef EXTERNAL_TEST_H
#define EXTERNAL_TEST_H

/*
  External tests: HPC overshadow, BFS partial, concurrency.
*/

int  external_test_get_count(void);
const char* external_test_get_name(int index);
int  external_test_run_single(int index);

void run_external_tests(void);

#endif
