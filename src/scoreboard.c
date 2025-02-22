#include "scoreboard.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int unlocked_basic=1;
int unlocked_normal=0;
int unlocked_external=0;
int unlocked_modes=0;
int unlocked_edge=0;
int unlocked_hidden=0;

static scoreboard_t gSB={0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60.0};
static sqlite3* g_db=NULL;

static void update_unlocks(void){
    gSB.basic_percent  = gSB.basic_total>0?  (gSB.basic_pass*100.0 / gSB.basic_total) :0.0;
    gSB.normal_percent = gSB.normal_total>0? (gSB.normal_pass*100.0 / gSB.normal_total):0.0;
    gSB.modes_percent  = gSB.modes_total>0?  (gSB.modes_pass*100.0  / gSB.modes_total) :0.0;
    gSB.edge_percent   = gSB.edge_total>0?   (gSB.edge_pass*100.0   / gSB.edge_total)  :0.0;
    gSB.hidden_percent = gSB.hidden_total>0? (gSB.hidden_pass*100.0 / gSB.hidden_total):0.0;
    double T=gSB.pass_threshold;
    if(gSB.basic_percent>=T){ unlocked_normal=1; unlocked_external=1;}
    if(gSB.normal_percent>=T) unlocked_modes=1;
    if(gSB.modes_percent>=T) unlocked_edge=1;
    if(gSB.edge_percent>=T) unlocked_hidden=1;
}

void scoreboard_init_db(void){
    if(g_db) return;
    if(sqlite3_open("./scoreboard.db",&g_db)!=SQLITE_OK){ g_db=NULL; return; }
    const char* ct="CREATE TABLE IF NOT EXISTS scoreboard("
                    "id INTEGER PRIMARY KEY,"
                    "basic_total INT,basic_pass INT,"
                    "normal_total INT,normal_pass INT,"
                    "modes_total INT,modes_pass INT,"
                    "edge_total INT,edge_pass INT,"
                    "hidden_total INT,hidden_pass INT,"
                    "sc_fifo INT,sc_rr INT,sc_cfs INT,sc_cfs_srtf INT,sc_bfs INT,"
                    "sc_sjf INT,sc_strf INT,sc_hrrn INT,sc_hrrn_rt INT,"
                    "sc_priority INT,sc_hpc_over INT,sc_mlfq INT,"
                    "basic_percent REAL,normal_percent REAL,modes_percent REAL,"
                    "edge_percent REAL,hidden_percent REAL,pass_threshold REAL);";
    sqlite3_exec(g_db,ct,NULL,NULL,NULL);
    const char* ins="INSERT OR IGNORE INTO scoreboard(id,basic_total,basic_pass,normal_total,normal_pass,"
                    "modes_total,modes_pass,edge_total,edge_pass,hidden_total,hidden_pass,sc_fifo,sc_rr,sc_cfs,"
                    "sc_cfs_srtf,sc_bfs,sc_sjf,sc_strf,sc_hrrn,sc_hrrn_rt,sc_priority,sc_hpc_over,sc_mlfq,"
                    "basic_percent,normal_percent,modes_percent,edge_percent,hidden_percent,pass_threshold)"
                    " VALUES(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60);";
    sqlite3_exec(g_db,ins,NULL,NULL,NULL);
}

void scoreboard_close_db(void){
    if(g_db){ sqlite3_close(g_db); g_db=NULL;}
}

void scoreboard_load(void){
    if(!g_db) return;
    sqlite3_stmt* st=NULL;
    const char* sql="SELECT * FROM scoreboard WHERE id=1;";
    if(sqlite3_prepare_v2(g_db,sql,-1,&st,NULL)!=SQLITE_OK) return;
    if(sqlite3_step(st)==SQLITE_ROW){
        gSB.basic_total=sqlite3_column_int(st,1);
        gSB.basic_pass=sqlite3_column_int(st,2);
        gSB.normal_total=sqlite3_column_int(st,3);
        gSB.normal_pass=sqlite3_column_int(st,4);
        gSB.modes_total=sqlite3_column_int(st,5);
        gSB.modes_pass=sqlite3_column_int(st,6);
        gSB.edge_total=sqlite3_column_int(st,7);
        gSB.edge_pass=sqlite3_column_int(st,8);
        gSB.hidden_total=sqlite3_column_int(st,9);
        gSB.hidden_pass=sqlite3_column_int(st,10);
        gSB.sc_fifo=sqlite3_column_int(st,11);
        gSB.sc_rr=sqlite3_column_int(st,12);
        gSB.sc_cfs=sqlite3_column_int(st,13);
        gSB.sc_cfs_srtf=sqlite3_column_int(st,14);
        gSB.sc_bfs=sqlite3_column_int(st,15);
        gSB.sc_sjf=sqlite3_column_int(st,16);
        gSB.sc_strf=sqlite3_column_int(st,17);
        gSB.sc_hrrn=sqlite3_column_int(st,18);
        gSB.sc_hrrn_rt=sqlite3_column_int(st,19);
        gSB.sc_priority=sqlite3_column_int(st,20);
        gSB.sc_hpc_over=sqlite3_column_int(st,21);
        gSB.sc_mlfq=sqlite3_column_int(st,22);
        gSB.basic_percent=sqlite3_column_double(st,23);
        gSB.normal_percent=sqlite3_column_double(st,24);
        gSB.modes_percent=sqlite3_column_double(st,25);
        gSB.edge_percent=sqlite3_column_double(st,26);
        gSB.hidden_percent=sqlite3_column_double(st,27);
        gSB.pass_threshold=sqlite3_column_double(st,28);
    }
    sqlite3_finalize(st);
    update_unlocks();
}

static void scoreboard_save_internal(void){
    if(!g_db) return;
    sqlite3_stmt* st=NULL;
    const char* sql="UPDATE scoreboard SET "
                    "basic_total=?,basic_pass=?,normal_total=?,normal_pass=?,"
                    "modes_total=?,modes_pass=?,edge_total=?,edge_pass=?,hidden_total=?,hidden_pass=?,"
                    "sc_fifo=?,sc_rr=?,sc_cfs=?,sc_cfs_srtf=?,sc_bfs=?,sc_sjf=?,sc_strf=?,sc_hrrn=?,sc_hrrn_rt=?,"
                    "sc_priority=?,sc_hpc_over=?,sc_mlfq=?,basic_percent=?,normal_percent=?,modes_percent=?,"
                    "edge_percent=?,hidden_percent=?,pass_threshold=? WHERE id=1;";
    if(sqlite3_prepare_v2(g_db,sql,-1,&st,NULL)!=SQLITE_OK) return;
    sqlite3_bind_int(st,1,gSB.basic_total); sqlite3_bind_int(st,2,gSB.basic_pass);
    sqlite3_bind_int(st,3,gSB.normal_total); sqlite3_bind_int(st,4,gSB.normal_pass);
    sqlite3_bind_int(st,5,gSB.modes_total); sqlite3_bind_int(st,6,gSB.modes_pass);
    sqlite3_bind_int(st,7,gSB.edge_total);  sqlite3_bind_int(st,8,gSB.edge_pass);
    sqlite3_bind_int(st,9,gSB.hidden_total);sqlite3_bind_int(st,10,gSB.hidden_pass);
    sqlite3_bind_int(st,11,gSB.sc_fifo); sqlite3_bind_int(st,12,gSB.sc_rr);
    sqlite3_bind_int(st,13,gSB.sc_cfs); sqlite3_bind_int(st,14,gSB.sc_cfs_srtf);
    sqlite3_bind_int(st,15,gSB.sc_bfs); sqlite3_bind_int(st,16,gSB.sc_sjf);
    sqlite3_bind_int(st,17,gSB.sc_strf); sqlite3_bind_int(st,18,gSB.sc_hrrn);
    sqlite3_bind_int(st,19,gSB.sc_hrrn_rt); sqlite3_bind_int(st,20,gSB.sc_priority);
    sqlite3_bind_int(st,21,gSB.sc_hpc_over); sqlite3_bind_int(st,22,gSB.sc_mlfq);
    sqlite3_bind_double(st,23,gSB.basic_percent); sqlite3_bind_double(st,24,gSB.normal_percent);
    sqlite3_bind_double(st,25,gSB.modes_percent); sqlite3_bind_double(st,26,gSB.edge_percent);
    sqlite3_bind_double(st,27,gSB.hidden_percent); sqlite3_bind_double(st,28,gSB.pass_threshold);
    sqlite3_step(st); sqlite3_finalize(st);
}

void scoreboard_save(void){
    update_unlocks();
    scoreboard_save_internal();
}

void scoreboard_clear(void){
    memset(&gSB,0,sizeof(gSB));
    gSB.pass_threshold=60.0;
    unlocked_basic=1; unlocked_normal=0; unlocked_external=0;
    unlocked_modes=0; unlocked_edge=0; unlocked_hidden=0;
    scoreboard_save_internal();
}

void get_scoreboard(scoreboard_t* out){
    if(out) *out=gSB;
}

void scoreboard_set_sc_mastered(scheduler_alg_t alg){
    switch(alg){
    case ALG_FIFO: gSB.sc_fifo=1; break;
    case ALG_RR: gSB.sc_rr=1; break;
    case ALG_CFS: gSB.sc_cfs=1; break;
    case ALG_CFS_SRTF: gSB.sc_cfs_srtf=1; break;
    case ALG_BFS: gSB.sc_bfs=1; break;
    case ALG_SJF: gSB.sc_sjf=1; break;
    case ALG_STRF: gSB.sc_strf=1; break;
    case ALG_HRRN: gSB.sc_hrrn=1; break;
    case ALG_HRRN_RT: gSB.sc_hrrn_rt=1; break;
    case ALG_PRIORITY: gSB.sc_priority=1; break;
    case ALG_HPC_OVERSHADOW: gSB.sc_hpc_over=1; break;
    case ALG_MLFQ: gSB.sc_mlfq=1; break;
    default: break;
    }
}

void scoreboard_update_basic(int t,int p){ gSB.basic_total+=t; gSB.basic_pass+=p; }
void scoreboard_update_normal(int t,int p){ gSB.normal_total+=t; gSB.normal_pass+=p; }
void scoreboard_update_modes(int t,int p){ gSB.modes_total+=t; gSB.modes_pass+=p; }
void scoreboard_update_edge(int t,int p){ gSB.edge_total+=t; gSB.edge_pass+=p; }
void scoreboard_update_hidden(int t,int p){ gSB.hidden_total+=t; gSB.hidden_pass+=p; }
