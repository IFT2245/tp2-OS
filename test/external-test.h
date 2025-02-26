#ifndef EXTERNAL_TEST_H
#define EXTERNAL_TEST_H

/*
  External tests: HPC overshadow, BFS partial, concurrency.
*/

void run_external_tests(void);
int external_test_count(void);
void external_test_run_single(int i, int* pass_out);

#endif
