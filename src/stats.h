#ifndef STATS_H
#define STATS_H

/*
   stats.h => a global struct tracking concurrency usage, signals, tests, etc.
   Also color macros for convenient printing.
*/

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_RED     "\033[91m"
#define CLR_GREEN   "\033[92m"
#define CLR_GRAY    "\033[90m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"

typedef struct {
    int signals_received_sigint;
    int signals_received_sigterm;
    int signals_received_others;

    int concurrency_runs;
    int processes_spawned;
    int containers_created;
    int containers_removed;

    int tests_passed;
    int tests_failed;

    /* speed_mode: 0 => normal, 1 => fast */
    int speed_mode;

    /* track total concurrency commands issued in runner. */
    int concurrency_commands_run;

} stats_t;

/* Initialize global stats. */
void stats_init(void);

/* Get the global stats. */
void stats_get(stats_t* out);

/* set speed_mode (0 or 1). */
void stats_set_speed_mode(int mode);

/* get speed_mode. */
int stats_get_speed_mode(void);

/* increments for various tracked fields. */
void stats_inc_signal_sigint(void);
void stats_inc_signal_sigterm(void);
void stats_inc_signal_other(void);
void stats_inc_concurrency_runs(void);
void stats_inc_processes_spawned(void);
void stats_inc_containers_created(void);
void stats_inc_containers_removed(void);
void stats_inc_tests_passed(int count);
void stats_inc_tests_failed(int count);
void stats_inc_concurrency_commands(int n);

/* Print them in a block at program end. */
void stats_print_summary(void);

#endif
