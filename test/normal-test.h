#ifndef NORMAL_TEST_H
#define NORMAL_TEST_H

/*
  Normal tests: SJF, STRF, HRRN, HRRN-RT, PRIORITY, CFS-SRTF, etc.
*/

void run_normal_tests(int* total,int* passed);

/* For single-test picking. */
int         normal_test_get_count(void);
const char* normal_test_get_name(int index);
int         normal_test_run_single(int index);

#endif
