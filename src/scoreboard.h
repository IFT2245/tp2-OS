#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include "scheduler.h"

typedef struct {
    int basic_total,basic_pass;
    int normal_total,normal_pass;
    int modes_total,modes_pass;
    int edge_total,edge_pass;
    int hidden_total,hidden_pass;

    int sc_fifo, sc_rr, sc_cfs, sc_cfs_srtf, sc_bfs;
    int sc_sjf, sc_strf, sc_hrrn, sc_hrrn_rt;
    int sc_priority, sc_hpc_over, sc_mlfq;

    double basic_percent, normal_percent, modes_percent;
    double edge_percent, hidden_percent;
    double pass_threshold;
} scoreboard_t;

extern int unlocked_basic;
extern int unlocked_normal;
extern int unlocked_external;
extern int unlocked_modes;
extern int unlocked_edge;
extern int unlocked_hidden;

void scoreboard_init_db(void);
void scoreboard_close_db(void);
void scoreboard_load(void);
void scoreboard_save(void);
void scoreboard_clear(void);
void get_scoreboard(scoreboard_t* out);
void scoreboard_set_sc_mastered(scheduler_alg_t alg);
void scoreboard_update_basic(int total,int pass);
void scoreboard_update_normal(int total,int pass);
void scoreboard_update_modes(int total,int pass);
void scoreboard_update_edge(int total,int pass);
void scoreboard_update_hidden(int total,int pass);

#endif
