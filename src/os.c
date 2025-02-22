#include "os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_CONTAINERS 32

static uint64_t start_ms = 0;
static char container_paths[MAX_CONTAINERS][256];
static int container_count = 0;

/* Current monotonic time in ms */
static uint64_t now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (ts.tv_nsec / 1000000ULL);
}

void os_init(void){
    printf("\033[94mInit\033[0m\n");
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    start_ms = now_ms();
    memset(container_paths, 0, sizeof(container_paths));
    container_count = 0;
}

void os_cleanup(void){
    while(container_count > 0){
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
    char tmpl[] = "/tmp/os_container_XXXXXX";
    if(mkdtemp(tmpl)){
        strncpy(container_paths[container_count], tmpl, 255);
        container_paths[container_count][255] = '\0';
        container_count++;
        os_log("Container created");
    }
}

void os_remove_ephemeral_container(void){
    if(container_count <= 0) return;
    container_count--;
    const char* path = container_paths[container_count];
    if(path[0]){
        /* Attempt recursive removal (if needed) or simple rmdir */
        rmdir(path);
        memset(container_paths[container_count], 0, sizeof(container_paths[container_count]));
        os_log("Container removed");
    }
}

static void* overshadow_thread(void* arg){
    long* res = (long*)arg;
    long s = 0;
    for(long i=0; i<1000000; i++){
        s += (i % 17) + (i % 11);
    }
    *res = s;
    return NULL;
}

void os_run_hpc_overshadow(void){
    uint64_t t0 = os_time();
    printf("HPC overshadow start\n");
    int n = 4;
    pthread_t* th = (pthread_t*)malloc((size_t)n * sizeof(pthread_t));
    long* vals = (long*)calloc((size_t)n, sizeof(long));
    if(!th || !vals){
        free(th);
        free(vals);
        fprintf(stderr,"[os_run_hpc_overshadow] Allocation error\n");
        return;
    }
    for(int i=0; i<n; i++){
        pthread_create(&th[i], NULL, overshadow_thread, &vals[i]);
    }
    for(int i=0; i<n; i++){
        pthread_join(th[i], NULL);
    }
    free(th);
    free(vals);
    printf("HPC overshadow done\n");
    uint64_t t1 = os_time() - t0;
    char buf[128];
    snprintf(buf, sizeof(buf), "HPC overshadow total time: %llu ms", (unsigned long long)t1);
    os_log(buf);
}

void os_pipeline_example(void){
    uint64_t t0 = os_time();
    printf("Pipeline start\n");
    pid_t c1 = fork();
    if(c1 == 0){
        /* simulate pipeline stage with 50ms sleep */
        usleep(50000);
        _exit(0);
    }
    waitpid(c1, NULL, 0);
    printf("Pipeline end\n");
    uint64_t t1 = os_time() - t0;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "Pipeline total time: %llu ms", (unsigned long long)t1);
    os_log(tmp);
}

void os_run_distributed_example(void){
    uint64_t t0 = os_time();
    printf("Distributed example: fork\n");
    pid_t c = fork();
    if(c == 0){
        printf("Child distributed HPC overshadow\n");
        os_run_hpc_overshadow();
        _exit(0);
    } else if(c > 0){
        waitpid(c, NULL, 0);
    }
    uint64_t t1 = os_time() - t0;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "Distributed total time: %llu ms", (unsigned long long)t1);
    os_log(tmp);
}
