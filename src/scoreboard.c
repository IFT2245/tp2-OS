#include "scoreboard.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/cJSON.h"

/* Our single scoreboard instance. */
static scoreboard_t gSB;

static double calc_percent(int total, int pass){
    if(total == 0) return 0.0;
    return 100.0 * ((double)pass / (double)total);
}

static void recompute(void){
    gSB.basic_percent        = calc_percent(gSB.basic_total,      gSB.basic_pass);
    gSB.normal_percent       = calc_percent(gSB.normal_total,     gSB.normal_pass);
    gSB.edge_percent         = calc_percent(gSB.edge_total,       gSB.edge_pass);
    gSB.hidden_percent       = calc_percent(gSB.hidden_total,     gSB.hidden_pass);
    gSB.wfq_percent          = calc_percent(gSB.wfq_total,        gSB.wfq_pass);
    gSB.multi_hpc_percent    = calc_percent(gSB.multi_hpc_total,  gSB.multi_hpc_pass);
    gSB.bfs_percent          = calc_percent(gSB.bfs_total,        gSB.bfs_pass);
    gSB.mlfq_percent         = calc_percent(gSB.mlfq_total,       gSB.mlfq_pass);
    gSB.prio_preempt_percent = calc_percent(gSB.prio_preempt_total, gSB.prio_preempt_pass);
    gSB.hpc_bfs_percent      = calc_percent(gSB.hpc_bfs_total,    gSB.hpc_bfs_pass);
}

/* Weighted final score example:
   BASIC=10%, NORMAL=10%, EDGE=10%, HIDDEN=10%,
   WFQ=10%, MULTI_HPC=10%, BFS=10%, MLFQ=10%,
   PRIO_PREEMPT=10%, HPC_BFS=10%.
   HPC bonus=+10 if sc_hpc=1, but capped at 100 total.
*/
int scoreboard_get_final_score(void){
    recompute();

    double b    = gSB.basic_percent         * 0.10;
    double n    = gSB.normal_percent        * 0.10;
    double e    = gSB.edge_percent          * 0.10;
    double hi   = gSB.hidden_percent        * 0.10;
    double wf   = gSB.wfq_percent           * 0.10;
    double mh   = gSB.multi_hpc_percent     * 0.10;
    double bf   = gSB.bfs_percent           * 0.10;
    double ml   = gSB.mlfq_percent          * 0.10;
    double pp   = gSB.prio_preempt_percent  * 0.10;
    double hb   = gSB.hpc_bfs_percent       * 0.10;

    double HPC  = (gSB.sc_hpc ? 10.0 : 0.0);

    double total = b + n + e + hi + wf + mh + bf + ml + pp + hb + HPC;
    if(total > 100.0) total = 100.0;

    return (int)(total + 0.5);
}

static void scoreboard_defaults(void){
    memset(&gSB, 0, sizeof(gSB));
    gSB.pass_threshold = 60.0;
}

void scoreboard_load(void){
    scoreboard_defaults();
    FILE* f = fopen("scoreboard.json", "rb");
    if(!f){
        log_warn("No scoreboard.json => defaults");
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if(sz<0){
        fclose(f);
        return;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz+1);
    if(!buf){
        fclose(f);
        return;
    }
    if(fread(buf,1,sz,f) != (size_t)sz){
        free(buf);
        fclose(f);
        return;
    }
    buf[sz] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if(!root){
        log_warn("scoreboard parse fail => defaults");
        return;
    }
    #define JGETINT(_o,_n,_v) do{ cJSON*_tmp=cJSON_GetObjectItemCaseSensitive(_o,_n);\
        if(_tmp && cJSON_IsNumber(_tmp)) (_v)=_tmp->valueint;}while(0)
    #define JGETDBL(_o,_n,_v) do{ cJSON*_tmp=cJSON_GetObjectItemCaseSensitive(_o,_n);\
        if(_tmp && cJSON_IsNumber(_tmp)) (_v)=_tmp->valuedouble;}while(0)

    JGETINT(root,"basic_total",           gSB.basic_total);
    JGETINT(root,"basic_pass",            gSB.basic_pass);
    JGETINT(root,"normal_total",          gSB.normal_total);
    JGETINT(root,"normal_pass",           gSB.normal_pass);
    JGETINT(root,"edge_total",            gSB.edge_total);
    JGETINT(root,"edge_pass",             gSB.edge_pass);
    JGETINT(root,"hidden_total",          gSB.hidden_total);
    JGETINT(root,"hidden_pass",           gSB.hidden_pass);
    JGETINT(root,"wfq_total",             gSB.wfq_total);
    JGETINT(root,"wfq_pass",              gSB.wfq_pass);
    JGETINT(root,"multi_hpc_total",       gSB.multi_hpc_total);
    JGETINT(root,"multi_hpc_pass",        gSB.multi_hpc_pass);
    JGETINT(root,"bfs_total",             gSB.bfs_total);
    JGETINT(root,"bfs_pass",              gSB.bfs_pass);
    JGETINT(root,"mlfq_total",            gSB.mlfq_total);
    JGETINT(root,"mlfq_pass",             gSB.mlfq_pass);
    JGETINT(root,"prio_preempt_total",    gSB.prio_preempt_total);
    JGETINT(root,"prio_preempt_pass",     gSB.prio_preempt_pass);
    JGETINT(root,"hpc_bfs_total",         gSB.hpc_bfs_total);
    JGETINT(root,"hpc_bfs_pass",          gSB.hpc_bfs_pass);

    JGETINT(root,"sc_hpc",                gSB.sc_hpc);
    JGETDBL(root,"pass_threshold",        gSB.pass_threshold);

    cJSON_Delete(root);
    log_info("Scoreboard loaded");
}

void scoreboard_save(void){
    cJSON* root = cJSON_CreateObject();
    #define JADDINT(_o,_n,_v) cJSON_AddNumberToObject(_o,_n,(double)(_v))
    #define JADDDBL(_o,_n,_v) cJSON_AddNumberToObject(_o,_n,(_v))

    JADDINT(root,"basic_total",        gSB.basic_total);
    JADDINT(root,"basic_pass",         gSB.basic_pass);
    JADDINT(root,"normal_total",       gSB.normal_total);
    JADDINT(root,"normal_pass",        gSB.normal_pass);
    JADDINT(root,"edge_total",         gSB.edge_total);
    JADDINT(root,"edge_pass",          gSB.edge_pass);
    JADDINT(root,"hidden_total",       gSB.hidden_total);
    JADDINT(root,"hidden_pass",        gSB.hidden_pass);
    JADDINT(root,"wfq_total",          gSB.wfq_total);
    JADDINT(root,"wfq_pass",           gSB.wfq_pass);
    JADDINT(root,"multi_hpc_total",    gSB.multi_hpc_total);
    JADDINT(root,"multi_hpc_pass",     gSB.multi_hpc_pass);
    JADDINT(root,"bfs_total",          gSB.bfs_total);
    JADDINT(root,"bfs_pass",           gSB.bfs_pass);
    JADDINT(root,"mlfq_total",         gSB.mlfq_total);
    JADDINT(root,"mlfq_pass",          gSB.mlfq_pass);
    JADDINT(root,"prio_preempt_total", gSB.prio_preempt_total);
    JADDINT(root,"prio_preempt_pass",  gSB.prio_preempt_pass);
    JADDINT(root,"hpc_bfs_total",      gSB.hpc_bfs_total);
    JADDINT(root,"hpc_bfs_pass",       gSB.hpc_bfs_pass);

    JADDINT(root,"sc_hpc",             gSB.sc_hpc);
    JADDDBL(root,"pass_threshold",     gSB.pass_threshold);

    char* out = cJSON_Print(root);
    cJSON_Delete(root);

    FILE* f = fopen("scoreboard.json","wb");
    if(!f){
        log_error("Cannot write scoreboard.json");
        free(out);
        return;
    }
    fwrite(out, 1, strlen(out), f);
    fclose(f);
    free(out);
    log_info("Scoreboard saved");
}

void scoreboard_clear(void){
    scoreboard_defaults();
    scoreboard_save();
}

void scoreboard_update_basic(int t,int p){
    gSB.basic_total += t;
    gSB.basic_pass  += p;
}
void scoreboard_update_normal(int t,int p){
    gSB.normal_total += t;
    gSB.normal_pass  += p;
}
void scoreboard_update_edge(int t,int p){
    gSB.edge_total  += t;
    gSB.edge_pass   += p;
}
void scoreboard_update_hidden(int t,int p){
    gSB.hidden_total += t;
    gSB.hidden_pass  += p;
}
void scoreboard_update_wfq(int t,int p){
    gSB.wfq_total += t;
    gSB.wfq_pass  += p;
}
void scoreboard_update_multi_hpc(int t,int p){
    gSB.multi_hpc_total += t;
    gSB.multi_hpc_pass  += p;
}
void scoreboard_update_bfs(int t,int p){
    gSB.bfs_total += t;
    gSB.bfs_pass  += p;
}
void scoreboard_update_mlfq(int t,int p){
    gSB.mlfq_total += t;
    gSB.mlfq_pass  += p;
}
void scoreboard_update_prio_preempt(int t,int p){
    gSB.prio_preempt_total += t;
    gSB.prio_preempt_pass  += p;
}
void scoreboard_update_hpc_bfs(int t,int p){
    gSB.hpc_bfs_total += t;
    gSB.hpc_bfs_pass  += p;
}

void scoreboard_set_sc_hpc(int v){
    gSB.sc_hpc = (v ? 1 : 0);
}

/*
   Unlock logic can be used if you want gating rules
   (not strictly used in the new design).
*/
int scoreboard_is_unlocked(scoreboard_suite_t s){
    recompute();
    const double T = gSB.pass_threshold;
    switch(s){
        case SUITE_BASIC:       return 1;
        case SUITE_NORMAL:      return (gSB.basic_percent >= T);
        case SUITE_EDGE:        return (gSB.normal_percent >= T);
        case SUITE_HIDDEN:      return (gSB.edge_percent >= T);
        case SUITE_WFQ:         return (gSB.hidden_percent >= T);
        case SUITE_MULTI_HPC:   return (gSB.wfq_percent   >= T);
        case SUITE_BFS:         return (gSB.normal_percent>= T);
        case SUITE_MLFQ:        return (gSB.normal_percent>= T);
        case SUITE_PRIO_PREEMPT:return (gSB.edge_percent  >= T);
        case SUITE_HPC_BFS:     return (gSB.hidden_percent>= T);
        default: return 0;
    }
}

void get_scoreboard(scoreboard_t* out){
    if(out) *out = gSB;
}

void show_scoreboard(void){
    scoreboard_t sb;
    get_scoreboard(&sb);
    int final = scoreboard_get_final_score();

    printf("\n===== SCOREBOARD =====\n");
    printf(" BASIC     => %d/%d => %.1f%%\n",
         sb.basic_pass, sb.basic_total, sb.basic_percent);
    printf(" NORMAL    => %d/%d => %.1f%%\n",
         sb.normal_pass, sb.normal_total, sb.normal_percent);
    printf(" EDGE      => %d/%d => %.1f%%\n",
         sb.edge_pass, sb.edge_total, sb.edge_percent);
    printf(" HIDDEN    => %d/%d => %.1f%%\n",
         sb.hidden_pass, sb.hidden_total, sb.hidden_percent);
    printf(" WFQ       => %d/%d => %.1f%%\n",
         sb.wfq_pass, sb.wfq_total, sb.wfq_percent);
    printf(" MULTI_HPC => %d/%d => %.1f%%\n",
         sb.multi_hpc_pass, sb.multi_hpc_total, sb.multi_hpc_percent);
    printf(" BFS       => %d/%d => %.1f%%\n",
         sb.bfs_pass, sb.bfs_total, sb.bfs_percent);
    printf(" MLFQ      => %d/%d => %.1f%%\n",
         sb.mlfq_pass, sb.mlfq_total, sb.mlfq_percent);
    printf(" PRIO_PREEMPT => %d/%d => %.1f%%\n",
         sb.prio_preempt_pass, sb.prio_preempt_total, sb.prio_preempt_percent);
    printf(" HPC_BFS   => %d/%d => %.1f%%\n",
         sb.hpc_bfs_pass, sb.hpc_bfs_total, sb.hpc_bfs_percent);
    printf(" HPC Bonus => %s\n", (sb.sc_hpc ? "YES" : "NO"));
    printf(" Final Weighted Score => %d\n", final);
    printf("=======================\n");
}
