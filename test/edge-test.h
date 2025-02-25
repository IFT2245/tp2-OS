#ifndef EDGE_TEST_H
#define EDGE_TEST_H

/* Edge tests: extreme bursts, HPC under load, container spam, pipeline, etc. */

void run_edge_tests(int* total,int* passed);

/* Single-test picking */
int         edge_test_get_count(void);
const char* edge_test_get_name(int index);
int         edge_test_run_single(int index);

#endif
