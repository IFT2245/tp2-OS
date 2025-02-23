#ifndef EDGE_TEST_H
#define EDGE_TEST_H

/*
  run_edge_tests():
    Various edge cases: extreme long/short bursts, HPC under load,
    container spam, pipeline edge, multi-distributed runs, etc.
*/
void run_edge_tests(int* total,int* passed);

#endif
