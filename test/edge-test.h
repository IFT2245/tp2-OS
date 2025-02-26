#ifndef EDGE_TEST_H
#define EDGE_TEST_H

/* Edge tests: extreme bursts, HPC under load, container spam, pipeline, etc. */

void run_edge_tests(int* total,int* passed);
int edge_test_count(void);
void edge_test_run_single(int i, int* pass_out);

#endif
