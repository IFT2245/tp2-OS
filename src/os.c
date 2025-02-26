#include "os.h"
#include "stats.h"

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
    clock_gettime(CLOCK_MONOTONIC, &ts);
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
    setvbuf(stderr, NULL, _IONBF, 0);

    g_start_ms = now_ms();
    g_container_count = 0;
    memset(g_container_paths, 0, sizeof(g_container_paths));

    /* OS init block in normal mode */
    printf(CLR_BOLD CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf("║               OS INIT COMPLETE               ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");

    if(stats_get_speed_mode() == 0) {
        usleep(200000); /* short pause in normal mode */
    }
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

            if(stats_get_speed_mode()==0) {
                usleep(200000);
            }
        }
    }
    printf(CLR_BOLD CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf(               "║             OS CLEANUP COMPLETE              ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");

    if(stats_get_speed_mode() == 0) {
        usleep(250000);
    }
}

uint64_t os_time(void) {
    uint64_t now = now_ms();
    if(now < g_start_ms) return 0ULL;
    return (now - g_start_ms);
}

/* Minimal logging with a small delay in normal mode only */
void os_log(const char* msg) {
    if(!msg) return;
    if(stats_get_speed_mode() == 0) {
        printf("%s\n", msg);
        usleep(150000);
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

        if(stats_get_speed_mode()==0) {
            usleep(250000);
        }
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

        if(stats_get_speed_mode()==0) {
            usleep(250000);
        }
    }
}

/* HPC overshadow => spawn CPU-bound threads */
static void* overshadow_thread(void* arg) {
    long *ret = (long*)arg;
    long sum = 0;
    for (long i=0; i<700000; i++) {
        if(os_concurrency_stop_requested()) break;
        sum += (i % 17) + (i % 11);
    }
    *ret = sum;
    return NULL;
}

/* HPC overshadow: run multiple CPU-bound threads to show concurrency. */
void os_run_hpc_overshadow(void) {
    if(os_concurrency_stop_requested()) {
        printf(CLR_YELLOW "[OS] concurrency stop => skipping HPC overshadow.\n" CLR_RESET);
        return;
    }

    int n=4; /* 4 CPU-bound threads */
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

    printf(CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf("║       HPC-OVERSHADOW BLOCK END               ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");

    if(stats_get_speed_mode()==0){
        usleep(200000);
    }
}

/* Pipeline => single child fork + wait. */
void os_pipeline_example(void) {
    printf(CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");

    if(os_concurrency_stop_requested()) {
        printf(CLR_YELLOW "[OS] concurrency stop => skipping pipeline.\n" CLR_RESET);
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
