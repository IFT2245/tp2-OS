#ifndef BASIC_TEST_H
#define BASIC_TEST_H

/*
  Basic tests: FIFO, RR, CFS, BFS, pipeline, distributed, etc.
  We also provide single-test picking.
*/

void run_basic_tests(int* total,int* passed);

/* For single-test picking in a sub-sub menu: */
int         basic_test_get_count(void);
const char* basic_test_get_name(int index);
int         basic_test_run_single(int index);

#endif
