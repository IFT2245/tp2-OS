#ifndef EXTERNAL_TEST_H
#define EXTERNAL_TEST_H

/*
  EXTERNAL TEST SUITE:
   - 12 tests total, one for each scheduling mode:
       0=FIFO,1=RR,2=CFS,3=CFS-SRTF,4=BFS,
       5=SJF,6=STRF,7=HRRN,8=HRRN-RT,
       9=PRIORITY,10=HPC-OVER,11=MLFQ
   - If the external shell binary is missing, all tests fail.
   - Each test spawns shell-tp1-implementation with a small "sleep" concurrency scenario
     via stdin, ensuring the external scheduling is tested.
*/

void run_external_tests(int* total, int* passed);

/* Number of external tests (12). */
int  external_test_count(void);

/* Runs exactly one external test with index i (0..11). pass_out=1 if pass, else 0. */
void external_test_run_single(int i, int* pass_out);

void concurrency_test_case1(void);
void concurrency_test_case2(void);

#endif
