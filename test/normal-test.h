#ifndef NORMAL_TEST_H
#define NORMAL_TEST_H

/* Normal tests: SJF, STRF, HRRN, HRRN-RT, PRIORITY, CFS-SRTF, etc. */

void run_normal_tests(int* total,int* passed);
int normal_test_count(void);
void normal_test_run_single(int i, int* pass_out);
#endif
