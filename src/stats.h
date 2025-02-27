#ifndef STATS_H
#define STATS_H
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Colors for ASCII art convenience. */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_RED     "\033[91m"
#define CLR_GREEN   "\033[92m"
#define CLR_GRAY    "\033[90m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"

void test_set_fail_reason(const char* msg);
const char* test_get_fail_reason(void);


/*
   stats.h => a global struct tracking concurrency usage, signals, tests, etc.
   Also color macros for convenient printing.
*/

typedef struct {
    int tests_passed;
    int tests_failed;
    /* speed_mode: 0 => normal, 1 => fast */
    int speed_mode;
    int test_fifo;
    int test_rr;
    int test_cfs;
    int test_bfs;
    int test_pipeline;
    int test_distributed;
    int test_fifo_strict;

    int sjf;
    int strf;
    int hrrn;
    int hrrn_rt;
    int priority;
    int cfs_srtf;
    int sjf_strict;

    int hpc_over;
    int multi_containers;
    int multi_distrib;
    int pipeline_modes;
    int mix_algos;
    int double_hpc;
    int mlfq_check;


    char ** test_fifo_reason;
    char ** test_rr_reason;
    char ** test_cfs_reason;
    char ** test_bfs_reason;
} stats_t;

void stats_set_test_fifo(int mode);
int stats_get_test_fifo(void);
void stats_set_fifo_reason(const char* msg);
const char* stats_get_fifo_reason(void);


/* Initialize global stats. */
void stats_init(void);
/* Get the global stats. */
void stats_get(stats_t* out);
/* set speed_mode (0 or 1). */
void stats_set_speed_mode(int mode);
/* get speed_mode. */
int stats_get_speed_mode(void);
void stats_inc_tests_passed(int count);
void stats_inc_tests_failed(int count);
/* Print them in a block at program end. */
void stats_print_summary(void);

#endif
