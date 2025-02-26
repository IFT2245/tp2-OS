#ifndef BASIC_TEST_H
#define BASIC_TEST_H

/*
  Basic tests: FIFO, RR, CFS, BFS, pipeline, distributed, etc.
  We also provide single-test picking if needed.
*/

void run_basic_tests(int* total,int* passed);
int basic_test_count(void);
void basic_test_run_single(int i, int* pass_out);

#endif
