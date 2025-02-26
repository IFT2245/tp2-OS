#ifndef MODES_TEST_H
#define MODES_TEST_H

/* Modes test: HPC overshadow, BFS, MLFQ, pipeline, containers, etc.*/

void run_modes_tests(int* total,int* passed);
int modes_test_count(void);
void modes_test_run_single(int i, int* pass_out);

#endif
