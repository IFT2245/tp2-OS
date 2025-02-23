#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include "os.h"

#define MAX_CONTAINERS 32

#define CLR_CYAN    "\033[96m"
#define CLR_GREEN   "\033[92m"
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"

static uint64_t start_ms=0;
static char container_paths[MAX_CONTAINERS][256];
static int container_count=0;

static uint64_t now_ms(void){
    /* Real-time measurement for ASCII printing timestamps only. */
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

    printf(CLR_BOLD CLR_MAGENTA "==== OS INIT ====" CLR_RESET "\n");
    /* small delay so we can see output clearly */
    usleep(250000);
}

void os_cleanup(void){
    while(container_count>0){
        os_remove_ephemeral_container();
        usleep(200000);
    }
    printf(CLR_BOLD CLR_MAGENTA "==== OS CLEANUP ====" CLR_RESET "\n");
    usleep(250000);
}

uint64_t os_time(void){
    /* Return how many ms since we started the OS. This is purely for
       printing/logging, not for scheduling stats. */
    return now_ms() - start_ms;
}

void os_log(const char* msg){
    if(msg) {
        printf("%s\n", msg);
        usleep(150000);
    }
}

void os_create_ephemeral_container(void){
    if(container_count >= MAX_CONTAINERS) return;
    char tmpl[]="/tmp/os_cont_XXXXXX";
    if(mkdtemp(tmpl)){
        strncpy(container_paths[container_count], tmpl, 255);
        container_count++;
        printf(CLR_CYAN "$$$$$ Container created: %s (count=%d) $$$$$" CLR_RESET "\n",
               tmpl, container_count);
        usleep(250000);
    }
}

void os_remove_ephemeral_container(void){
    if(container_count<=0) return;
    container_count--;
    const char* path=container_paths[container_count];
    if(path[0]){
        rmdir(path);
        memset(container_paths[container_count], 0, sizeof(container_paths[container_count]));
        printf(CLR_CYAN "$$$$$ Container removed: %s (remaining=%d) $$$$$" CLR_RESET "\n",
               path, container_count);
        usleep(250000);
    }
}

/* overshadow_thread simulates HPC overshadowing by heavy CPU. */
static void* overshadow_thread(void* arg){
    long* ret=(long*)arg;
    long s=0;
    for(long i=0; i<700000; i++){
        s += (i%17)+(i%11);
    }
    *ret=s;
    return NULL;
}

void os_run_hpc_overshadow(void){
    printf(CLR_CYAN "$$$$$$$$$$$$$ HPC-OVERSHADOW BLOCK START $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);

    int n=4;
    long* vals=(long*)calloc(n,sizeof(long));
    pthread_t* th=(pthread_t*)malloc(n*sizeof(pthread_t));

    for(int i=0;i<n;i++){
        pthread_create(&th[i],NULL,overshadow_thread,&vals[i]);
        printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => started.\n" CLR_RESET,
               i+1, (unsigned long long)os_time());
        usleep(300000);
    }

    for(int i=0;i<n;i++){
        pthread_join(th[i],NULL);
        printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => finished.\n" CLR_RESET,
               i+1, (unsigned long long)os_time());
        usleep(300000);
    }

    free(th);
    free(vals);

    printf(CLR_CYAN "$$$$$$$$$$$$$ HPC-OVERSHADOW BLOCK END $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);
    printf("HPC overshadow done\n");
    usleep(200000);
}

void os_pipeline_example(void){
    printf("Pipeline start\n");
    printf(CLR_CYAN "$$$$$$$$$$$ PIPELINE BLOCK START $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);

    pid_t c=fork();
    if(c==0){
        printf(CLR_GREEN "   [Pipeline child => started => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        usleep(50000);
        printf(CLR_GREEN "   [Pipeline child => finishing => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        _exit(0);
    }
    printf(CLR_GREEN "   [Pipeline parent => waiting child => time=%llu ms]\n" CLR_RESET,
           (unsigned long long)os_time());
    usleep(200000);
    waitpid(c,NULL,0);

    printf(CLR_CYAN "$$$$$$$$$$$ PIPELINE BLOCK END $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);
    printf("Pipeline end\n");
    usleep(200000);
}

void os_run_distributed_example(void){
    printf("Distributed example: fork\n");
    printf(CLR_CYAN "$$$$$$$ DISTRIBUTED BLOCK START $$$$$$$$\n" CLR_RESET);
    usleep(200000);

    pid_t c=fork();
    if(c==0){
        printf(CLR_GREEN "   [Distributed child => HPC overshadow => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        usleep(200000);
        os_run_hpc_overshadow();
        _exit(0);
    } else {
        printf(CLR_GREEN "   [Distributed parent => waiting => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        usleep(200000);
        waitpid(c,NULL,0);
    }

    printf(CLR_CYAN "$$$$$$$ DISTRIBUTED BLOCK END $$$$$$$$\n" CLR_RESET);
    usleep(200000);
}
