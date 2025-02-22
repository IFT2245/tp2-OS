#ifndef RUNNER_H
#define RUNNER_H

/* Unlock flags for main menu */
extern int unlocked_basic;
extern int unlocked_normal;
extern int unlocked_external;
extern int unlocked_modes;
extern int unlocked_edge;
extern int unlocked_hidden;

/* Scoreboard struct with everything we want to show in main menu */
typedef struct {
    int basic_total,   basic_pass;
    int normal_total,  normal_pass;
    int modes_total,   modes_pass;
    int edge_total,    edge_pass;
    int hidden_total,  hidden_pass;
    double basic_percent;
    double normal_percent;
    double modes_percent;
    double edge_percent;
    double hidden_percent;
    double pass_threshold; /* 60% by default */
} scoreboard_t;

/* Fills 'out' with the current scoreboard data */
void get_scoreboard(scoreboard_t* out);

/* Runs all unlocked levels in a row */
void run_all_levels(void);

/* Runs external tests (if unlocked) */
void run_external_tests_menu(void);

/* Launch user lines concurrently on simulated cores. */
void run_shell_commands_concurrently(int count, char** lines, int coreCount);

#endif
