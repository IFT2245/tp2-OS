#include "os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <fcntl.h>

static uint64_t start_ms=0;
static char container_path[256]={0};

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*1000ULL + (ts.tv_nsec/1000000ULL);
}

void os_init(void) {
    start_ms=now_ms();
    os_log("Init");
}

void os_cleanup(void) {
    os_remove_ephemeral_container();
    os_log("Cleanup");
}

uint64_t os_time(void) {
    return now_ms()-start_ms;
}

void os_log(const char* msg) {
    fprintf(stdout,"[OS] %s\n", msg);
}

void os_create_ephemeral_container(void) {
    if(strlen(container_path)==0) {
        char tmpl[]="/tmp/os_container_XXXXXX";
        if(mkdtemp(tmpl)) {
            strncpy(container_path,tmpl,sizeof(container_path)-1);
            os_log("Container created");
        }
    }
}

void os_remove_ephemeral_container(void) {
    if(strlen(container_path)) {
        rmdir(container_path);
        os_log("Container removed");
        memset(container_path,0,sizeof(container_path));
    }
}

static void* overshadow_func(void* arg) {
    long* res=(long*)arg;
    long sum=0;
    for(long i=0;i<1000000;i++){
        sum+=(i%17)+(i%11);
    }
    *res=sum;
    return NULL;
}

void os_run_hpc_overshadow(void) {
    os_log("HPC overshadow start");
    int nth=4;
    pthread_t *arr=(pthread_t*)malloc(nth*sizeof(pthread_t));
    long *vals=(long*)calloc(nth,sizeof(long));
    for(int i=0;i<nth;i++) {
        pthread_create(&arr[i],NULL,overshadow_func,&vals[i]);
    }
    for(int i=0;i<nth;i++) {
        pthread_join(arr[i],NULL);
    }
    free(arr);
    free(vals);
    os_log("HPC overshadow done");
}

/*
  A pipeline in ephemeral container if possible:
  - Stage1 partial HPC
  - Stage2 partial HPC
  - Stage3 sums results
*/
void os_pipeline_example(void) {
    os_log("Pipeline start");
    os_create_ephemeral_container();

    char container_file[512];
    snprintf(container_file,sizeof(container_file),"%s/pipe_data.txt",container_path);
    int pipe1[2],pipe2[2];
    pipe(pipe1); pipe(pipe2);

    pid_t stg1=fork();
    if(stg1==0){
        close(pipe1[0]);
        long sum1=0;
        for(long i=0;i<300000;i++) sum1+=(i%7);
        dprintf(pipe1[1],"%ld\n",sum1);
        close(pipe1[1]);
        _exit(0);
    } else if(stg1<0) { os_log("fork stg1 fail"); }

    pid_t stg2=fork();
    if(stg2==0){
        close(pipe1[1]);
        close(pipe2[0]);
        long in1=0;
        fscanf(fdopen(pipe1[0],"r"),"%ld",&in1);
        close(pipe1[0]);
        long sum2=0;
        for(long i=0;i<200000;i++) sum2+=(i%5);
        long total = in1+sum2;
        dprintf(pipe2[1],"%ld\n",total);
        close(pipe2[1]);
        _exit(0);
    } else if(stg2<0) { os_log("fork stg2 fail"); }

    close(pipe1[0]); close(pipe1[1]);
    pid_t stg3=fork();
    if(stg3==0){
        close(pipe2[1]);
        long in2=0;
        fscanf(fdopen(pipe2[0],"r"),"%ld",&in2);
        close(pipe2[0]);
        int fd=open(container_file,O_WRONLY|O_CREAT,0644);
        if(fd>=0){
            dprintf(fd,"Final pipeline sum: %ld\n",in2);
            close(fd);
        }
        _exit(0);
    } else if(stg3<0){ os_log("fork stg3 fail"); }

    close(pipe2[0]); close(pipe2[1]);
    waitpid(stg1,NULL,0);
    waitpid(stg2,NULL,0);
    waitpid(stg3,NULL,0);

    FILE* f=fopen(container_file,"r");
    if(f) {
        char buf[128];
        if(fgets(buf,sizeof(buf),f)) {
            os_log(buf);
        }
        fclose(f);
    }
    os_remove_ephemeral_container();
    os_log("Pipeline end");
}

void os_run_distributed_example(void){
    os_log("Distributed example: fork");
    pid_t c=fork();
    if(c==0){
        os_log("Child distributed HPC overshadow");
        os_run_hpc_overshadow();
        _exit(0);
    } else if(c>0){
        waitpid(c,NULL,0);
    }
}
