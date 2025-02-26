#ifndef HIDDEN_TEST_H
#define HIDDEN_TEST_H

/* Hidden tests: synergy HPC + containers + distributed, advanced scheduling variety, etc. */

void run_hidden_tests(int* total,int* passed);
int hidden_test_count(void);
void hidden_test_run_single(int i, int* pass_out);
#endif
