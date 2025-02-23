#include "scoreboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int unlocked_basic=1;
int unlocked_normal=0;
int unlocked_external=0;
int unlocked_modes=0;
int unlocked_edge=0;
int unlocked_hidden=0;

static scoreboard_t gSB={
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,
    0.0,0.0,0.0,0.0,0.0,0.0,
    60.0
};

static void update_unlocks(void){
    gSB.basic_percent=(gSB.basic_total>0)?
        ((gSB.basic_pass*100.0)/gSB.basic_total):0.0;
    gSB.normal_percent=(gSB.normal_total>0)?
        ((gSB.normal_pass*100.0)/gSB.normal_total):0.0;
    gSB.external_percent=(gSB.external_total>0)?
        ((gSB.external_pass*100.0)/gSB.external_total):0.0;
    gSB.modes_percent=(gSB.modes_total>0)?
        ((gSB.modes_pass*100.0)/gSB.modes_total):0.0;
    gSB.edge_percent=(gSB.edge_total>0)?
        ((gSB.edge_pass*100.0)/gSB.edge_total):0.0;
    gSB.hidden_percent=(gSB.hidden_total>0)?
        ((gSB.hidden_pass*100.0)/gSB.hidden_total):0.0;

    double T=gSB.pass_threshold;
    if(gSB.basic_percent>=T){
        unlocked_normal=1;
        unlocked_external=1;
    }
    if(gSB.normal_percent>=T){
        unlocked_modes=1;
    }
    if(gSB.modes_percent>=T){
        unlocked_edge=1;
    }
    if(gSB.edge_percent>=T){
        unlocked_hidden=1;
    }
}

static char* read_file_all(const char* path){
    FILE* f=fopen(path,"rb");
    if(!f) return NULL;
    fseek(f,0,SEEK_END);
    long size=ftell(f);
    if(size<0){fclose(f);return NULL;}
    fseek(f,0,SEEK_SET);
    char* buf=(char*)malloc(size+1);
    if(!buf){fclose(f);return NULL;}
    if(fread(buf,1,size,f)!=(size_t)size){
        fclose(f); free(buf); return NULL;
    }
    buf[size]='\0';
    fclose(f);
    return buf;
}

static int parse_json_int(const char* json,const char* key,int def){
    if(!json||!key) return def;
    char pattern[128];
    snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    char* found=strstr(json,pattern);
    if(!found) return def;
    char* colon=strstr(found,":");
    if(!colon) return def;
    colon++;
    while(*colon && (*colon==' '||*colon=='\t')) colon++;
    int val=def;
    sscanf(colon,"%d",&val);
    return val;
}

static double parse_json_double(const char* json,const char* key,double def){
    if(!json||!key) return def;
    char pattern[128];
    snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    char* found=strstr(json,pattern);
    if(!found) return def;
    char* colon=strstr(found,":");
    if(!colon) return def;
    colon++;
    while(*colon && (*colon==' '||*colon=='\t')) colon++;
    double val=def;
    sscanf(colon,"%lf",&val);
    return val;
}

static void write_scoreboard_json(const scoreboard_t* sb){
    FILE* f=fopen("scoreboard.json","w");
    if(!f) return;
    fprintf(f,"{\n");
    fprintf(f,"  \"basic_total\": %d,\n",   sb->basic_total);
    fprintf(f,"  \"basic_pass\": %d,\n",    sb->basic_pass);
    fprintf(f,"  \"normal_total\": %d,\n",  sb->normal_total);
    fprintf(f,"  \"normal_pass\": %d,\n",   sb->normal_pass);
    fprintf(f,"  \"external_total\": %d,\n",sb->external_total);
    fprintf(f,"  \"external_pass\": %d,\n", sb->external_pass);
    fprintf(f,"  \"modes_total\": %d,\n",   sb->modes_total);
    fprintf(f,"  \"modes_pass\": %d,\n",    sb->modes_pass);
    fprintf(f,"  \"edge_total\": %d,\n",    sb->edge_total);
    fprintf(f,"  \"edge_pass\": %d,\n",     sb->edge_pass);
    fprintf(f,"  \"hidden_total\": %d,\n",  sb->hidden_total);
    fprintf(f,"  \"hidden_pass\": %d,\n",   sb->hidden_pass);

    fprintf(f,"  \"sc_fifo\": %d,\n",       sb->sc_fifo);
    fprintf(f,"  \"sc_rr\": %d,\n",         sb->sc_rr);
    fprintf(f,"  \"sc_cfs\": %d,\n",        sb->sc_cfs);
    fprintf(f,"  \"sc_cfs_srtf\": %d,\n",   sb->sc_cfs_srtf);
    fprintf(f,"  \"sc_bfs\": %d,\n",        sb->sc_bfs);
    fprintf(f,"  \"sc_sjf\": %d,\n",        sb->sc_sjf);
    fprintf(f,"  \"sc_strf\": %d,\n",       sb->sc_strf);
    fprintf(f,"  \"sc_hrrn\": %d,\n",       sb->sc_hrrn);
    fprintf(f,"  \"sc_hrrn_rt\": %d,\n",    sb->sc_hrrn_rt);
    fprintf(f,"  \"sc_priority\": %d,\n",   sb->sc_priority);
    fprintf(f,"  \"sc_hpc_over\": %d,\n",   sb->sc_hpc_over);
    fprintf(f,"  \"sc_mlfq\": %d,\n",       sb->sc_mlfq);

    fprintf(f,"  \"basic_percent\": %.3f,\n",   sb->basic_percent);
    fprintf(f,"  \"normal_percent\": %.3f,\n",  sb->normal_percent);
    fprintf(f,"  \"external_percent\": %.3f,\n",sb->external_percent);
    fprintf(f,"  \"modes_percent\": %.3f,\n",   sb->modes_percent);
    fprintf(f,"  \"edge_percent\": %.3f,\n",    sb->edge_percent);
    fprintf(f,"  \"hidden_percent\": %.3f,\n",  sb->hidden_percent);

    fprintf(f,"  \"pass_threshold\": %.1f\n",   sb->pass_threshold);
    fprintf(f,"}\n");
    fclose(f);
}

void scoreboard_init(void){}
void scoreboard_close(void){}

void scoreboard_load(void){
    char* json=read_file_all("scoreboard.json");
    if(!json){
        update_unlocks();
        return;
    }
    gSB.basic_total   =parse_json_int(json,"basic_total",   gSB.basic_total);
    gSB.basic_pass    =parse_json_int(json,"basic_pass",    gSB.basic_pass);
    gSB.normal_total  =parse_json_int(json,"normal_total",  gSB.normal_total);
    gSB.normal_pass   =parse_json_int(json,"normal_pass",   gSB.normal_pass);
    gSB.external_total=parse_json_int(json,"external_total",gSB.external_total);
    gSB.external_pass =parse_json_int(json,"external_pass", gSB.external_pass);
    gSB.modes_total   =parse_json_int(json,"modes_total",   gSB.modes_total);
    gSB.modes_pass    =parse_json_int(json,"modes_pass",    gSB.modes_pass);
    gSB.edge_total    =parse_json_int(json,"edge_total",    gSB.edge_total);
    gSB.edge_pass     =parse_json_int(json,"edge_pass",     gSB.edge_pass);
    gSB.hidden_total  =parse_json_int(json,"hidden_total",  gSB.hidden_total);
    gSB.hidden_pass   =parse_json_int(json,"hidden_pass",   gSB.hidden_pass);

    gSB.sc_fifo       =parse_json_int(json,"sc_fifo",       gSB.sc_fifo);
    gSB.sc_rr         =parse_json_int(json,"sc_rr",         gSB.sc_rr);
    gSB.sc_cfs        =parse_json_int(json,"sc_cfs",        gSB.sc_cfs);
    gSB.sc_cfs_srtf   =parse_json_int(json,"sc_cfs_srtf",   gSB.sc_cfs_srtf);
    gSB.sc_bfs        =parse_json_int(json,"sc_bfs",        gSB.sc_bfs);
    gSB.sc_sjf        =parse_json_int(json,"sc_sjf",        gSB.sc_sjf);
    gSB.sc_strf       =parse_json_int(json,"sc_strf",       gSB.sc_strf);
    gSB.sc_hrrn       =parse_json_int(json,"sc_hrrn",       gSB.sc_hrrn);
    gSB.sc_hrrn_rt    =parse_json_int(json,"sc_hrrn_rt",    gSB.sc_hrrn_rt);
    gSB.sc_priority   =parse_json_int(json,"sc_priority",   gSB.sc_priority);
    gSB.sc_hpc_over   =parse_json_int(json,"sc_hpc_over",   gSB.sc_hpc_over);
    gSB.sc_mlfq       =parse_json_int(json,"sc_mlfq",       gSB.sc_mlfq);

    gSB.basic_percent   =parse_json_double(json,"basic_percent",   gSB.basic_percent);
    gSB.normal_percent  =parse_json_double(json,"normal_percent",  gSB.normal_percent);
    gSB.external_percent=parse_json_double(json,"external_percent",gSB.external_percent);
    gSB.modes_percent   =parse_json_double(json,"modes_percent",   gSB.modes_percent);
    gSB.edge_percent    =parse_json_double(json,"edge_percent",    gSB.edge_percent);
    gSB.hidden_percent  =parse_json_double(json,"hidden_percent",  gSB.hidden_percent);

    gSB.pass_threshold  =parse_json_double(json,"pass_threshold",  gSB.pass_threshold);

    free(json);
    update_unlocks();
}

void scoreboard_save(void){
    update_unlocks();
    write_scoreboard_json(&gSB);
}

/*
 Weighted test sets:
   BASIC=20%, NORMAL=20%, EXTERNAL=20%,
   MODES=10%, EDGE=15%, HIDDEN=15% => 100 total
 Each mastered scheduler => +1.5 up to 100 max
*/
int scoreboard_get_final_score(void){
    update_unlocks();
    double wb=0.20, wn=0.20, we=0.20;
    double wm=0.10, wedge=0.15, wh=0.15;

    double fs=(gSB.basic_percent*wb)
             +(gSB.normal_percent*wn)
             +(gSB.external_percent*we)
             +(gSB.modes_percent*wm)
             +(gSB.edge_percent*wedge)
             +(gSB.hidden_percent*wh);

    /* +1.5% per mastered mode. */
    if(gSB.sc_fifo)        fs+=1.5;
    if(gSB.sc_rr)          fs+=1.5;
    if(gSB.sc_cfs)         fs+=1.5;
    if(gSB.sc_cfs_srtf)    fs+=1.5;
    if(gSB.sc_bfs)         fs+=1.5;
    if(gSB.sc_sjf)         fs+=1.5;
    if(gSB.sc_strf)        fs+=1.5;
    if(gSB.sc_hrrn)        fs+=1.5;
    if(gSB.sc_hrrn_rt)     fs+=1.5;
    if(gSB.sc_priority)    fs+=1.5;
    if(gSB.sc_hpc_over)    fs+=1.5;
    if(gSB.sc_mlfq)        fs+=1.5;

    if(fs>100.) fs=100.;
    if(fs<0.)   fs=0.;
    int final_i=(int)(fs+0.5);
    if(final_i>100) final_i=100;
    if(final_i<0)   final_i=0;
    return final_i;
}

void scoreboard_clear(void){
    memset(&gSB,0,sizeof(gSB));
    gSB.pass_threshold=60.0;
    unlocked_basic=1;
    unlocked_normal=0;
    unlocked_external=0;
    unlocked_modes=0;
    unlocked_edge=0;
    unlocked_hidden=0;
    scoreboard_save();
}

void get_scoreboard(scoreboard_t* out){
    if(out) *out=gSB;
}

/* Mark a scheduling mode as mastered => sets scoreboard bit => final score +1.5%. */
void scoreboard_set_sc_mastered(scheduler_alg_t alg){
    switch(alg){
    case ALG_FIFO:          gSB.sc_fifo=1;        break;
    case ALG_RR:            gSB.sc_rr=1;          break;
    case ALG_CFS:           gSB.sc_cfs=1;         break;
    case ALG_CFS_SRTF:      gSB.sc_cfs_srtf=1;    break;
    case ALG_BFS:           gSB.sc_bfs=1;         break;
    case ALG_SJF:           gSB.sc_sjf=1;         break;
    case ALG_STRF:          gSB.sc_strf=1;        break;
    case ALG_HRRN:          gSB.sc_hrrn=1;        break;
    case ALG_HRRN_RT:       gSB.sc_hrrn_rt=1;     break;
    case ALG_PRIORITY:      gSB.sc_priority=1;    break;
    case ALG_HPC_OVERSHADOW:gSB.sc_hpc_over=1;    break;
    case ALG_MLFQ:          gSB.sc_mlfq=1;        break;
    default: break;
    }
}

void scoreboard_update_basic(int total,int pass){
    gSB.basic_total+=total;
    gSB.basic_pass+=pass;
}

void scoreboard_update_normal(int total,int pass){
    gSB.normal_total+=total;
    gSB.normal_pass+=pass;
}

void scoreboard_update_external(int total,int pass){
    gSB.external_total+=total;
    gSB.external_pass+=pass;
}

void scoreboard_update_modes(int total,int pass){
    gSB.modes_total+=total;
    gSB.modes_pass+=pass;
}

void scoreboard_update_edge(int total,int pass){
    gSB.edge_total+=total;
    gSB.edge_pass+=pass;
}

void scoreboard_update_hidden(int total,int pass){
    gSB.hidden_total+=total;
    gSB.hidden_pass+=pass;
}
