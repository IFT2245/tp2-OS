#ifndef SCOREBOARD_H
#define SCOREBOARD_H

typedef enum {
    SUITE_BASIC=1,
    SUITE_NORMAL,
    SUITE_EDGE,
    SUITE_HIDDEN,
    SUITE_WFQ,
    SUITE_MULTI_HPC,
    SUITE_BFS,
    SUITE_MLFQ,
    SUITE_PRIO_PREEMPT,
    SUITE_HPC_BFS
} scoreboard_suite_t;

/**
 * @brief The scoreboard structure. Now the simpler tests weigh more.
 */
typedef struct {
    int basic_total,      basic_pass;
    int normal_total,     normal_pass;
    int edge_total,       edge_pass;
    int hidden_total,     hidden_pass;
    int wfq_total,        wfq_pass;
    int multi_hpc_total,  multi_hpc_pass;
    int bfs_total,        bfs_pass;
    int mlfq_total,       mlfq_pass;
    int prio_preempt_total, prio_preempt_pass;
    int hpc_bfs_total,      hpc_bfs_pass;

    double basic_percent,
           normal_percent,
           edge_percent,
           hidden_percent,
           wfq_percent,
           multi_hpc_percent,
           bfs_percent,
           mlfq_percent,
           prio_preempt_percent,
           hpc_bfs_percent;

    double pass_threshold; /* e.g. 60% */
    int    sc_hpc;         /* HPC bonus toggle => +5% if set */
} scoreboard_t;

/* Load/save scoreboard from scoreboard.json */
void scoreboard_load(void);
void scoreboard_save(void);
void scoreboard_clear(void);

/* Update suite test counts. */
void scoreboard_update_basic(int t,int p);
void scoreboard_update_normal(int t,int p);
void scoreboard_update_edge(int t,int p);
void scoreboard_update_hidden(int t,int p);
void scoreboard_update_wfq(int t,int p);
void scoreboard_update_multi_hpc(int t,int p);
void scoreboard_update_bfs(int t,int p);
void scoreboard_update_mlfq(int t,int p);
void scoreboard_update_prio_preempt(int t,int p);
void scoreboard_update_hpc_bfs(int t,int p);

/* HPC bonus on/off => +5% if sc_hpc=1. */
void scoreboard_set_sc_hpc(int v);

/* Gate logic: is suite unlocked after some threshold? */
int  scoreboard_is_unlocked(scoreboard_suite_t s);

/* Retrieve + compute final */
void get_scoreboard(scoreboard_t* out);
int  scoreboard_get_final_score(void);

/* Display */
void show_scoreboard(void);
void show_legend(void);

#endif
