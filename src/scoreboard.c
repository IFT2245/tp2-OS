#include "scoreboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* We'll keep a static scoreboard in memory. */
static scoreboard_t gSB = {
    0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,
    60.0 /* pass_threshold default */
};

/* Forward static function prototypes are not used; we define directly above usage. */

/* scoreboard_init: no-op */
void scoreboard_init(void) {
    /* no-op */
}

/* scoreboard_close: no-op */
void scoreboard_close(void) {
    /* no-op */
}

static char* read_file_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if(sz<0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz+1);
    if(!buf) {
        fclose(f);
        return NULL;
    }
    if(fread(buf,1,sz,f) != (size_t)sz) {
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static int parse_json_int(const char* json, const char* key, int def) {
    if(!json || !key) return def;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char* found = strstr(json, pattern);
    if(!found) return def;
    char* colon = strstr(found, ":");
    if(!colon) return def;
    colon++;
    while(*colon && (*colon==' '||*colon=='\t')) colon++;
    int val = def;
    sscanf(colon, "%d", &val);
    return val;
}

static double parse_json_double(const char* json, const char* key, double def) {
    if(!json || !key) return def;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char* found = strstr(json, pattern);
    if(!found) return def;
    char* colon = strstr(found, ":");
    if(!colon) return def;
    colon++;
    while(*colon && (*colon==' '||*colon=='\t')) colon++;
    double val = def;
    sscanf(colon, "%lf", &val);
    return val;
}

static void write_scoreboard_json(const scoreboard_t* sb) {
    FILE* f = fopen("scoreboard.json", "w");
    if(!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"basic_total\": %d,\n", sb->basic_total);
    fprintf(f, "  \"basic_pass\": %d,\n", sb->basic_pass);
    fprintf(f, "  \"normal_total\": %d,\n", sb->normal_total);
    fprintf(f, "  \"normal_pass\": %d,\n", sb->normal_pass);
    fprintf(f, "  \"external_total\": %d,\n", sb->external_total);
    fprintf(f, "  \"external_pass\": %d,\n", sb->external_pass);
    fprintf(f, "  \"modes_total\": %d,\n", sb->modes_total);
    fprintf(f, "  \"modes_pass\": %d,\n", sb->modes_pass);
    fprintf(f, "  \"edge_total\": %d,\n", sb->edge_total);
    fprintf(f, "  \"edge_pass\": %d,\n", sb->edge_pass);
    fprintf(f, "  \"hidden_total\": %d,\n", sb->hidden_total);
    fprintf(f, "  \"hidden_pass\": %d,\n", sb->hidden_pass);

    fprintf(f, "  \"sc_fifo\": %d,\n",      sb->sc_fifo);
    fprintf(f, "  \"sc_rr\": %d,\n",        sb->sc_rr);
    fprintf(f, "  \"sc_cfs\": %d,\n",       sb->sc_cfs);
    fprintf(f, "  \"sc_cfs_srtf\": %d,\n",  sb->sc_cfs_srtf);
    fprintf(f, "  \"sc_bfs\": %d,\n",       sb->sc_bfs);
    fprintf(f, "  \"sc_sjf\": %d,\n",       sb->sc_sjf);
    fprintf(f, "  \"sc_strf\": %d,\n",      sb->sc_strf);
    fprintf(f, "  \"sc_hrrn\": %d,\n",      sb->sc_hrrn);
    fprintf(f, "  \"sc_hrrn_rt\": %d,\n",   sb->sc_hrrn_rt);
    fprintf(f, "  \"sc_priority\": %d,\n",  sb->sc_priority);
    fprintf(f, "  \"sc_hpc_over\": %d,\n",  sb->sc_hpc_over);
    fprintf(f, "  \"sc_mlfq\": %d,\n",      sb->sc_mlfq);

    fprintf(f, "  \"basic_percent\": %.3f,\n",    sb->basic_percent);
    fprintf(f, "  \"normal_percent\": %.3f,\n",   sb->normal_percent);
    fprintf(f, "  \"external_percent\": %.3f,\n", sb->external_percent);
    fprintf(f, "  \"modes_percent\": %.3f,\n",    sb->modes_percent);
    fprintf(f, "  \"edge_percent\": %.3f,\n",     sb->edge_percent);
    fprintf(f, "  \"hidden_percent\": %.3f,\n",   sb->hidden_percent);

    fprintf(f, "  \"pass_threshold\": %.1f\n",    sb->pass_threshold);
    fprintf(f, "}\n");
    fclose(f);
}

void scoreboard_load(void) {
    char* json = read_file_all("scoreboard.json");
    if(!json) {
        /* update_unlocks below will recalc percentages => defaults. */
        return;
    }

    gSB.basic_total    = parse_json_int(json,"basic_total",    gSB.basic_total);
    gSB.basic_pass     = parse_json_int(json,"basic_pass",     gSB.basic_pass);
    gSB.normal_total   = parse_json_int(json,"normal_total",   gSB.normal_total);
    gSB.normal_pass    = parse_json_int(json,"normal_pass",    gSB.normal_pass);
    gSB.external_total = parse_json_int(json,"external_total", gSB.external_total);
    gSB.external_pass  = parse_json_int(json,"external_pass",  gSB.external_pass);
    gSB.modes_total    = parse_json_int(json,"modes_total",    gSB.modes_total);
    gSB.modes_pass     = parse_json_int(json,"modes_pass",     gSB.modes_pass);
    gSB.edge_total     = parse_json_int(json,"edge_total",     gSB.edge_total);
    gSB.edge_pass      = parse_json_int(json,"edge_pass",      gSB.edge_pass);
    gSB.hidden_total   = parse_json_int(json,"hidden_total",   gSB.hidden_total);
    gSB.hidden_pass    = parse_json_int(json,"hidden_pass",    gSB.hidden_pass);

    gSB.sc_fifo      = parse_json_int(json,"sc_fifo",      gSB.sc_fifo);
    gSB.sc_rr        = parse_json_int(json,"sc_rr",        gSB.sc_rr);
    gSB.sc_cfs       = parse_json_int(json,"sc_cfs",       gSB.sc_cfs);
    gSB.sc_cfs_srtf  = parse_json_int(json,"sc_cfs_srtf",  gSB.sc_cfs_srtf);
    gSB.sc_bfs       = parse_json_int(json,"sc_bfs",       gSB.sc_bfs);
    gSB.sc_sjf       = parse_json_int(json,"sc_sjf",       gSB.sc_sjf);
    gSB.sc_strf      = parse_json_int(json,"sc_strf",      gSB.sc_strf);
    gSB.sc_hrrn      = parse_json_int(json,"sc_hrrn",      gSB.sc_hrrn);
    gSB.sc_hrrn_rt   = parse_json_int(json,"sc_hrrn_rt",   gSB.sc_hrrn_rt);
    gSB.sc_priority  = parse_json_int(json,"sc_priority",  gSB.sc_priority);
    gSB.sc_hpc_over  = parse_json_int(json,"sc_hpc_over",  gSB.sc_hpc_over);
    gSB.sc_mlfq      = parse_json_int(json,"sc_mlfq",      gSB.sc_mlfq);

    gSB.basic_percent    = parse_json_double(json,"basic_percent",    gSB.basic_percent);
    gSB.normal_percent   = parse_json_double(json,"normal_percent",   gSB.normal_percent);
    gSB.external_percent = parse_json_double(json,"external_percent", gSB.external_percent);
    gSB.modes_percent    = parse_json_double(json,"modes_percent",    gSB.modes_percent);
    gSB.edge_percent     = parse_json_double(json,"edge_percent",     gSB.edge_percent);
    gSB.hidden_percent   = parse_json_double(json,"hidden_percent",   gSB.hidden_percent);
    gSB.pass_threshold   = parse_json_double(json,"pass_threshold",   gSB.pass_threshold);

    free(json);
}

/* Weighted pass check => if a suite's pass% >= pass_threshold => "unlocked". */
static int is_suite_unlocked(scoreboard_suite_t suite) {
    /* Recompute pass percents: */
    if(gSB.basic_total>0)
        gSB.basic_percent = (gSB.basic_pass  *100.0)/(double)gSB.basic_total;
    else
        gSB.basic_percent = 0.0;
    if(gSB.normal_total>0)
        gSB.normal_percent = (gSB.normal_pass *100.0)/(double)gSB.normal_total;
    else
        gSB.normal_percent = 0.0;
    if(gSB.external_total>0)
        gSB.external_percent = (gSB.external_pass*100.0)/(double)gSB.external_total;
    else
        gSB.external_percent = 0.0;
    if(gSB.modes_total>0)
        gSB.modes_percent = (gSB.modes_pass  *100.0)/(double)gSB.modes_total;
    else
        gSB.modes_percent = 0.0;
    if(gSB.edge_total>0)
        gSB.edge_percent = (gSB.edge_pass   *100.0)/(double)gSB.edge_total;
    else
        gSB.edge_percent = 0.0;
    if(gSB.hidden_total>0)
        gSB.hidden_percent = (gSB.hidden_pass *100.0)/(double)gSB.hidden_total;
    else
        gSB.hidden_percent = 0.0;

    double T = gSB.pass_threshold;

    switch(suite) {
    case SUITE_BASIC:
        return (gSB.basic_percent >= T) ? 1 : 0;
    case SUITE_NORMAL:
        return (gSB.normal_percent >= T) ? 1 : 0;
    case SUITE_EXTERNAL:
        return (gSB.external_percent >= T) ? 1 : 0;
    case SUITE_MODES:
        return (gSB.modes_percent >= T) ? 1 : 0;
    case SUITE_EDGE:
        return (gSB.edge_percent >= T) ? 1 : 0;
    case SUITE_HIDDEN:
        return (gSB.hidden_percent >= T) ? 1 : 0;
    default:
        return 0;
    }
}

void scoreboard_save(void) {
    /* Ensure our pass% is up to date before writing. */
    (void)is_suite_unlocked(SUITE_BASIC);
    write_scoreboard_json(&gSB);
}

void scoreboard_clear(void) {
    memset(&gSB, 0, sizeof(gSB));
    gSB.pass_threshold = 60.0;
    scoreboard_save();
}

void get_scoreboard(scoreboard_t* out) {
    if(out) *out = gSB;
}

static int get_scheduler_points(void) {
    /* BFS=2, HPC=2, MLFQ=2, others=1 => up to 15 total. */
    int points = 0;
    if(gSB.sc_fifo)         points += 1;
    if(gSB.sc_rr)           points += 1;
    if(gSB.sc_cfs)          points += 1;
    if(gSB.sc_cfs_srtf)     points += 1;
    if(gSB.sc_bfs)          points += 2;
    if(gSB.sc_sjf)          points += 1;
    if(gSB.sc_strf)         points += 1;
    if(gSB.sc_hrrn)         points += 1;
    if(gSB.sc_hrrn_rt)      points += 1;
    if(gSB.sc_priority)     points += 1;
    if(gSB.sc_hpc_over)     points += 2;
    if(gSB.sc_mlfq)         points += 2;
    return points;
}

/* Weighted blocks:
   basic => 32%
   normal => 20%
   external => 10%
   modes => 10%
   edge => 10%
   hidden => 8%
   sched mastery => 10%
*/
int scoreboard_get_final_score(void) {
    /* Recompute pass% in is_suite_unlocked: */
    (void)is_suite_unlocked(SUITE_BASIC);

    double b  = gSB.basic_percent     * 0.32;
    double n  = gSB.normal_percent    * 0.20;
    double e  = gSB.external_percent  * 0.10;
    double m  = gSB.modes_percent     * 0.10;
    double ed = gSB.edge_percent      * 0.10;
    double h  = gSB.hidden_percent    * 0.08;

    int sched_pts = get_scheduler_points();
    double sched_percent = ((double)sched_pts / 15.0)*100.0;
    double s = sched_percent * 0.10; /* 10% of overall */

    double total = b + n + e + m + ed + h + s;
    if(total>100.0) total=100.0;
    if(total<0.0)   total=0.0;
    return (int)(total+0.5);
}

int scoreboard_is_unlocked(scoreboard_suite_t suite) {
    return is_suite_unlocked(suite);
}

void scoreboard_set_sc_mastered(scheduler_alg_t alg) {
    switch(alg) {
    case ALG_FIFO:          gSB.sc_fifo      = 1; break;
    case ALG_RR:            gSB.sc_rr        = 1; break;
    case ALG_CFS:           gSB.sc_cfs       = 1; break;
    case ALG_CFS_SRTF:      gSB.sc_cfs_srtf  = 1; break;
    case ALG_BFS:           gSB.sc_bfs       = 1; break;
    case ALG_SJF:           gSB.sc_sjf       = 1; break;
    case ALG_STRF:          gSB.sc_strf      = 1; break;
    case ALG_HRRN:          gSB.sc_hrrn      = 1; break;
    case ALG_HRRN_RT:       gSB.sc_hrrn_rt   = 1; break;
    case ALG_PRIORITY:      gSB.sc_priority  = 1; break;
    case ALG_HPC_OVERSHADOW:gSB.sc_hpc_over  = 1; break;
    case ALG_MLFQ:          gSB.sc_mlfq      = 1; break;
    default: break;
    }
}

void scoreboard_update_basic(int total, int pass) {
    gSB.basic_total += total;
    gSB.basic_pass  += pass;
}
void scoreboard_update_normal(int total, int pass) {
    gSB.normal_total += total;
    gSB.normal_pass  += pass;
}
void scoreboard_update_external(int total,int pass) {
    gSB.external_total += total;
    gSB.external_pass  += pass;
}
void scoreboard_update_modes(int total,int pass) {
    gSB.modes_total += total;
    gSB.modes_pass  += pass;
}
void scoreboard_update_edge(int total,int pass) {
    gSB.edge_total += total;
    gSB.edge_pass  += pass;
}
void scoreboard_update_hidden(int total,int pass) {
    gSB.hidden_total += total;
    gSB.hidden_pass  += pass;
}
