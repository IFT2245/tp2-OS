#include "os.h"
#include "stats.h"
#include "safe_calls_library.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

/*
  Global concurrency stop flag. If set, any concurrency loop stops early.
*/
static volatile sig_atomic_t g_concurrency_stop_flag = 0;

/* For measuring time since os_init() */
static uint64_t g_start_ms = 0;

/* Up to 32 ephemeral containers. */
static int       g_container_count = 0;
static char      g_container_paths[32][256];

/* Returns monotonic current time in ms */
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); // `CLOCK_MONOTONIC` is a system clock that always moves forward and is not affected by system clock changes (e.g., daylight savings or manual changes to the system time).
    /*
    - `ts.tv_sec`: The number of **full seconds** elapsed since an unspecified epoch
    - `ts.tv_nsec`: The number of **nanoseconds** (subsecond part) since the last full second
    - Multiply `ts.tv_sec` (seconds) by `1000ULL` to convert seconds to milliseconds
    - Divide `ts.tv_nsec` (nanoseconds) by `1,000,000` to convert nanoseconds to milliseconds
    */
    return (uint64_t)(ts.tv_sec)*1000ULL + (ts.tv_nsec / 1000000ULL);
}

void set_os_concurrency_stop_flag(int val) {
    g_concurrency_stop_flag = (sig_atomic_t)val;
}

int os_concurrency_stop_requested(void) {
    return (int)g_concurrency_stop_flag;
}

/* ----------------------------------------------------------------
   Initialize "OS"
   ----------------------------------------------------------------
*/
void os_init(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0); // no buffer to flush
    g_start_ms = now_ms();
    g_container_count = 0;
    memset(g_container_paths, 0, sizeof(g_container_paths));
}

/* ----------------------------------------------------------------
   Cleanup: remove ephemeral containers, etc.
   ----------------------------------------------------------------
*/
void os_cleanup(void) {
    while (g_container_count > 0) {
        g_container_count--;
        const char* path = g_container_paths[g_container_count];
        if (path && path[0]) {
            rmdir(path);
            memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
            printf(CLR_CYAN "[-] Container removed (cleanup): %s\n" CLR_RESET, path);
            stats_inc_containers_removed();
        }
    }
}

uint64_t os_time(void) {
    const uint64_t now = now_ms();
    if(now < g_start_ms) return 0ULL; // 0 unsigned Long Long
    return (now - g_start_ms);
}

/* Minimal logging with a small delay in normal mode only */
void os_log(const char* msg) {
    if(!msg) return;
    if(stats_get_speed_mode() == 0) {
        printf("%s\n", msg);
        usleep(500000);
    }
}

/* ----------------------------------------------------------------
   Container creation
   ----------------------------------------------------------------
*/
void os_create_ephemeral_container(void) {
    if(g_container_count >= 32) return;
    char tmpl[] = "/tmp/os_cont_XXXXXX";
    if(mkdtemp(tmpl)) {
        strncpy(g_container_paths[g_container_count], tmpl, 255);
        g_container_count++;
        printf(CLR_CYAN "[+] Container created: %s (count=%d)\n" CLR_RESET,
               tmpl, g_container_count);
        stats_inc_containers_created();
    }
}

/* Container removal */
void os_remove_ephemeral_container(void) {
    if(g_container_count <= 0) return;
    g_container_count--;
    const char* path = g_container_paths[g_container_count];
    if(path[0]) {
        rmdir(path);
        memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
        printf(CLR_CYAN "[-] Container removed: %s (remaining=%d)\n" CLR_RESET,
               path, g_container_count);
        stats_inc_containers_removed();
    }
}

/* HPC overshadow => spawn CPU-bound threads */
static void* overshadow_thread(void* arg) {
    long *ret = (long*)arg;
    long sum = 0;
    for (long i=0; i<7000000; i++) {
        if(os_concurrency_stop_requested()) break;
        sum += (i % 17) + (i % 11);
    }
    *ret = sum;
    return NULL;
}

/* HPC overshadow: run multiple CPU-bound threads to show concurrency. */
void os_run_hpc_overshadow(void) {
    if(os_concurrency_stop_requested()) {
        printf(CLR_RED "[OS] concurrency has to stop\n" CLR_RESET);
        return;
    }

    const int n=4; /* 4 CPU-bound threads */
    long* results = (long*)calloc(n, sizeof(long));
    pthread_t* th = (pthread_t*)malloc(n*sizeof(pthread_t));

    for (int i=0; i<n; i++) {
        pthread_create(&th[i], NULL, overshadow_thread, &results[i]);
        if(stats_get_speed_mode()==0){
            printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => started.\n" CLR_RESET,
                   i+1, (unsigned long long)os_time());
            usleep(200000);
        }
    }
    for (int i=0; i<n; i++) {
        pthread_join(th[i], NULL);
        if(stats_get_speed_mode()==0){
            printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => finished.\n" CLR_RESET,
                   i+1, (unsigned long long)os_time());
            usleep(200000);
        }
    }

    free(th);
    free(results);
    if (stats_get_speed_mode()==0)
    {
        printf(CLR_GREEN "   HPC Overshadow => time=%llu ms => END.\n" CLR_RESET, (unsigned long long)os_time());
    }
}

/* Pipeline => single child fork + wait. */
void os_pipeline_example(void) {
    printf(CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");

    if(os_concurrency_stop_requested()) {
        printf(CLR_RED "[OS] concurrency stop => skipping pipeline.\n" CLR_RESET);
        return;
    }

    pid_t c = fork();
    if (c == 0) {
        if(stats_get_speed_mode()==0){
            printf(CLR_GREEN "   [Pipeline child => start => time=%llu ms]\n" CLR_RESET,
                   (unsigned long long)os_time());
            usleep(50000);
            printf(CLR_GREEN "   [Pipeline child => end => time=%llu ms]\n" CLR_RESET,
                   (unsigned long long)os_time());
        }
        _exit(0);
    } else if (c > 0) {
        if(stats_get_speed_mode()==0){
            printf(CLR_GREEN "   [Pipeline parent => waiting => time=%llu ms]\n" CLR_RESET,
                   (unsigned long long)os_time());
            usleep(200000);
        }
        waitpid(c, NULL, 0);
    }

    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK END               ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");

    if(stats_get_speed_mode()==0){
        usleep(200000);
    }
}

/* Distributed => fork child that runs HPC overshadow. */
void os_run_distributed_example(void) {
    printf(CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf("║          DISTRIBUTED BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");

    if(os_concurrency_stop_requested()) {
        printf(CLR_YELLOW "[OS] concurrency stop => skipping distributed.\n" CLR_RESET);
        return;
    }

    pid_t c = fork();
    if (c == 0) {
        if(stats_get_speed_mode()==0){
            printf(CLR_GREEN "   [Distributed child => HPC overshadow => time=%llu ms]\n" CLR_RESET,
                   (unsigned long long)os_time());
            usleep(200000);
        }
        os_run_hpc_overshadow();
        _exit(0);
    } else if (c > 0) {
        if(stats_get_speed_mode()==0){
            printf(CLR_GREEN "   [Distributed parent => waiting => time=%llu ms]\n" CLR_RESET,
                   (unsigned long long)os_time());
            usleep(200000);
        }
        waitpid(c, NULL, 0);
    }

    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║           DISTRIBUTED BLOCK END              ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
}
