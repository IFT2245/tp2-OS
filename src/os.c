#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "os.h"

#define MAX_CONTAINERS 32

static uint64_t start_ms=0;
static char container_paths[MAX_CONTAINERS][256];
static int container_count=0;

static uint64_t now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec*1000ULL + (ts.tv_nsec/1000000ULL);
}

void os_init(void){
    setvbuf(stdout,NULL,_IONBF,0);
    setvbuf(stderr,NULL,_IONBF,0);
    start_ms=now_ms();
    memset(container_paths, 0, sizeof(container_paths));
    container_count=0;
    printf("\033[94mInit\033[0m\n");
}

void os_cleanup(void){
    while(container_count>0){
        os_remove_ephemeral_container();
    }
    printf("\033[96mCleanup\033[0m\n");
}

uint64_t os_time(void){
    return now_ms() - start_ms;
}

void os_log(const char* msg){
    if(msg) printf("%s\n", msg);
}

void os_create_ephemeral_container(void){
    if(container_count >= MAX_CONTAINERS) return;
    char tmpl[]="/tmp/os_cont_XXXXXX";
    if(mkdtemp(tmpl)){
        strncpy(container_paths[container_count], tmpl, 255);
        container_count++;
        printf("Container created: %s\n", tmpl);
    }
}

void os_remove_ephemeral_container(void){
    if(container_count<=0) return;
    container_count--;
    const char* path=container_paths[container_count];
    if(path[0]){
        rmdir(path);
        memset(container_paths[container_count], 0, sizeof(container_paths[container_count]));
        printf("Container removed: %s\n", path);
    }
}

static void* overshadow_thread(void* arg){
    long* ret=(long*)arg;
    long s=0;
    for(long i=0; i<1000000; i++){
        s += (i%17)+(i%11);
    }
    *ret=s;
    return NULL;
}

void os_run_hpc_overshadow(void){
    printf("HPC overshadow start\n");
    int n=4;
    long* vals=(long*)calloc(n,sizeof(long));
    pthread_t* th=(pthread_t*)malloc(n*sizeof(pthread_t));
    for(int i=0;i<n;i++){
        pthread_create(&th[i],NULL,overshadow_thread,&vals[i]);
    }
    for(int i=0;i<n;i++){
        pthread_join(th[i],NULL);
    }
    free(th);
    free(vals);
    printf("HPC overshadow done\n");
}

void os_pipeline_example(void){
    printf("Pipeline start\n");
    pid_t c=fork();
    if(c==0){
        usleep(50000);
        _exit(0);
    }
    waitpid(c,NULL,0);
    printf("Pipeline end\n");
}

void os_run_distributed_example(void){
    printf("Distributed example: fork\n");
    pid_t c=fork();
    if(c==0){
        printf("Child HPC overshadow\n");
        os_run_hpc_overshadow();
        _exit(0);
    } else {
        waitpid(c,NULL,0);
    }
}
