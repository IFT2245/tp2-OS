#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include "scheduler.h"

/* Enum to identify a test suite. */
typedef enum {
    SUITE_BASIC = 1,
    SUITE_NORMAL,
    SUITE_EXTERNAL,
    SUITE_MODES,
    SUITE_EDGE,
    SUITE_HIDDEN,
    SUITE_COUNT
} scoreboard_suite_t;

/*
  Structure for scoreboard data.
*/
typedef struct {
    int basic_total,    basic_pass;
    int normal_total,   normal_pass;
    int external_total, external_pass;
    int modes_total,    modes_pass;
    int edge_total,     edge_pass;
    int hidden_total,   hidden_pass;

    /* Mastery flags for scheduling algorithms. */
    int sc_fifo;
    int sc_rr;
    int sc_cfs;
    int sc_cfs_srtf;
    int sc_bfs;
    int sc_sjf;
    int sc_strf;
    int sc_hrrn;
    int sc_hrrn_rt;
    int sc_priority;
    int sc_hpc_overshadow;   /* renamed from sc_hpc_over */
    int sc_mlfq;
    int sc_hpc_overlay;      /* newly added field */

    double basic_percent;
    double normal_percent;
    double external_percent;
    double modes_percent;
    double edge_percent;
    double hidden_percent;

    /* Passing threshold for unlocking next suite. */
    double pass_threshold;
} scoreboard_t;

/* Initialize scoreboard (no-op). */
void scoreboard_init(void);

/* Close scoreboard (no-op). */
void scoreboard_close(void);

/* Load from scoreboard.json -> internal scoreboard. */
void scoreboard_load(void);

/* Save scoreboard -> scoreboard.json. */
void scoreboard_save(void);

/* Clear scoreboard entirely. */
void scoreboard_clear(void);

/* Retrieve scoreboard snapshot. */
void get_scoreboard(scoreboard_t* out);

/* Returns final overall score (0..100). */
int  scoreboard_get_final_score(void);

/* Mark that we have "mastered" a scheduling algorithm. */
void scoreboard_set_sc_mastered(scheduler_alg_t alg);

/* Update raw counts for each test suite. */
void scoreboard_update_basic(int total, int pass);
void scoreboard_update_normal(int total, int pass);
void scoreboard_update_external(int total,int pass);
void scoreboard_update_modes(int total,int pass);
void scoreboard_update_edge(int total,int pass);
void scoreboard_update_hidden(int total,int pass);

/* Check if suite is unlocked. (Basic is always unlocked at game start.) */
int scoreboard_is_unlocked(scoreboard_suite_t suite);

#endif
