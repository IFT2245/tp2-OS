#ifndef HIDDEN_TEST_H
#define HIDDEN_TEST_H

/*
  Hidden tests: synergy HPC + containers + distributed,
  plus advanced scheduling variety.
*/

void run_hidden_tests(int* total,int* passed);

/* For sub-sub picking. */
int         hidden_test_get_count(void);
const char* hidden_test_get_name(int index);
int         hidden_test_run_single(int index);

#endif
