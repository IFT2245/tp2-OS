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

/* For measuring time since os_init() in real-time ms. */
static uint64_t g_start_ms = 0;

/* Up to 32 ephemeral containers. */
static int       g_container_count = 0;
static char      g_container_paths[32][256];

/* Returns monotonic current time in ms. */
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
}

/* ----------------------------------------------------------------
   Cleanup: remove ephemeral containers, etc.
   ----------------------------------------------------------------
*/
void os_cleanup(void) {
    /* Remove all ephemeral containers in reverse order. */
    while (g_container_count > 0) {
        g_container_count--;
        const char* path = g_container_paths[g_container_count];
        if (path && path[0]) {
            rmdir(path);
            memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
            printf("\033[96m[-] Container removed (cleanup): %s\n\033[0m", path);
            stats_inc_containers_removed();
        }
    }
}

uint64_t os_time(void) {
    /* Return real-world elapsed ms since os_init(). */
    const uint64_t now = now_ms();
    if(now < g_start_ms) return 0ULL;
    return (now - g_start_ms);
}

/* Minimal logging with a small delay in normal mode only. */
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
        printf("\033[96m[+] Container created: %s (count=%d)\n\033[0m",
               tmpl, g_container_count);
        stats_inc_containers_created();
    }
}

/* Container removal (explicit). */
void os_remove_ephemeral_container(void) {
    if(g_container_count <= 0) return;
    g_container_count--;
    const char* path = g_container_paths[g_container_count];
    if(path[0]) {
        rmdir(path);
        memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
        printf("\033[96m[-] Container removed: %s (remaining=%d)\n\033[0m",
               path, g_container_count);
        stats_inc_containers_removed();
    }
}

/*
   HPC overshadow => spawns multiple CPU-bound threads to demonstrate concurrency.
   Now refined to choose a random number of threads between 4..8.
*/
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

void os_run_hpc_overshadow(void) {
    if(os_concurrency_stop_requested()) {
        printf("\033[91m[OS] concurrency has to stop => HPC overshadow aborted\n\033[0m");
        return;
    }

    /* NEW/UPDATED: pick a random number of CPU-bound threads from 4..8. */
    srand((unsigned int)time(NULL));
    int n = 4 + (rand() % 5); /* range = [4..8]. */

    long* results = (long*)calloc(n, sizeof(long));
    pthread_t* th = (pthread_t*)malloc(n*sizeof(pthread_t));

    if(stats_get_speed_mode()==0){
        printf("\033[92m[HPC Overshadow] => spawning %d threads\n\033[0m", n);
        usleep(200000);
    }

    for (int i=0; i<n; i++) {
        pthread_create(&th[i], NULL, overshadow_thread, &results[i]);
        if(stats_get_speed_mode()==0){
            printf("\033[92m   HPC Overshadow Thread #%d => time=%llu ms => started.\n\033[0m",
                   i+1, (unsigned long long)os_time());
            usleep(150000);
        }
    }
    for (int i=0; i<n; i++) {
        pthread_join(th[i], NULL);
        if(stats_get_speed_mode()==0){
            printf("\033[92m   HPC Overshadow Thread #%d => time=%llu ms => finished.\n\033[0m",
                   i+1, (unsigned long long)os_time());
            usleep(100000);
        }
    }

    free(th);
    free(results);
    if (stats_get_speed_mode()==0) {
        printf("\033[92m   HPC Overshadow => time=%llu ms => END.\n\033[0m",
               (unsigned long long)os_time());
    }
}

/*
   HPC overlay => spawns a smaller random number of CPU-bound threads, e.g. [2..4].
*/
static void* overlay_thread(void* arg) {
    long *ret = (long*)arg;
    long sum = 0;
    for (long i=0; i<4000000; i++) {
        if(os_concurrency_stop_requested()) break;
        sum += (i % 19) + (i % 13);
    }
    *ret = sum;
    return NULL;
}

void os_run_hpc_overlay(void) {
    if(os_concurrency_stop_requested()) {
        printf("\033[91m[OS] concurrency stop => HPC overlay aborted\n\033[0m");
        return;
    }

    /* NEW/UPDATED: random number of threads in [2..4]. */
    srand((unsigned int)time(NULL));
    int n = 2 + (rand() % 3); /* range = [2..4]. */

    long* results = (long*)calloc(n, sizeof(long));
    pthread_t* th = (pthread_t*)malloc(n*sizeof(pthread_t));

    if(stats_get_speed_mode()==0){
        printf("\033[92m[HPC Overlay] => spawning %d threads\n\033[0m", n);
        usleep(200000);
    }

    for (int i=0; i<n; i++) {
        pthread_create(&th[i], NULL, overlay_thread, &results[i]);
        if(stats_get_speed_mode()==0){
            printf("\033[92m   HPC Overlay Thread #%d => time=%llu ms => started.\n\033[0m",
                   i+1, (unsigned long long)os_time());
            usleep(150000);
        }
    }
    for (int i=0; i<n; i++) {
        pthread_join(th[i], NULL);
        if(stats_get_speed_mode()==0){
            printf("\033[92m   HPC Overlay Thread #%d => time=%llu ms => finished.\n\033[0m",
                   i+1, (unsigned long long)os_time());
            usleep(100000);
        }
    }

    free(th);
    free(results);
    if (stats_get_speed_mode()==0) {
        printf("\033[92m   HPC Overlay => time=%llu ms => END.\n\033[0m",
               (unsigned long long)os_time());
    }
}

/*
   Pipeline example => now refined to create multiple child processes in series,
   simulating a multi-stage pipeline.
*/
void os_pipeline_example(void) {
    printf("\033[95m╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");

    if(os_concurrency_stop_requested()) {
        printf("\033[91m[OS] concurrency stop => skipping pipeline.\033[0m\n");
        return;
    }

    /* We'll do a 2-stage pipeline:
       stage1 child => does some "work"
       then pipe into stage2 child => does next "work"
       (in real shell code, you'd connect the outputs/inputs, but here we demonstrate the concept.)
    */
    int pipefd[2];
    if(pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid_t c1 = fork();
    if(c1==0) {
        /* Child1 => simulate pipeline stage #1 */
        close(pipefd[0]);
        if(stats_get_speed_mode()==0){
            printf("\033[92m   [Pipeline child1 => start => time=%llu ms]\033[0m\n",
                   (unsigned long long)os_time());
            usleep(100000);
        }
        /* write some data to pipe to pass to next stage. */
        const char* data="Hello from stage1";
        write(pipefd[1], data, strlen(data)+1);
        close(pipefd[1]);

        if(stats_get_speed_mode()==0){
            printf("\033[92m   [Pipeline child1 => end => time=%llu ms]\033[0m\n",
                   (unsigned long long)os_time());
        }
        _exit(0);
    } else if(c1>0) {
        /* parent => spawn stage2 */
        pid_t c2 = fork();
        if(c2==0) {
            close(pipefd[1]);
            /* read data from stage1. */
            char buf[128];
            memset(buf,0,sizeof(buf));
            read(pipefd[0], buf, sizeof(buf)-1);
            if(stats_get_speed_mode()==0){
                printf("\033[92m   [Pipeline child2 => got data=\"%s\" => time=%llu ms]\033[0m\n",
                       buf, (unsigned long long)os_time());
                usleep(150000);
            }
            close(pipefd[0]);
            _exit(0);
        } else if(c2>0) {
            /* parent => wait for both. */
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(c1,NULL,0);
            waitpid(c2,NULL,0);
        }
    }

    printf("\033[96m╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK END               ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");

    if(stats_get_speed_mode()==0){
        usleep(200000);
    }
}

/*
   Distributed example => we fork multiple HPC overshadow tasks in parallel
   to simulate "remote nodes" doing CPU-bound HPC overshadow.
*/
void os_run_distributed_example(void) {
    printf("\033[95m╔══════════════════════════════════════════════╗\n");
    printf("║          DISTRIBUTED BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");

    if(os_concurrency_stop_requested()) {
        printf("\033[93m[OS] concurrency stop => skipping distributed.\033[0m\n");
        return;
    }

    /* We'll spawn 2 HPC overshadow children, each doing overshadow. */
    for(int i=0; i<2; i++) {
        if(os_concurrency_stop_requested()) {
            break;
        }
        pid_t c = fork();
        if (c == 0) {
            if(stats_get_speed_mode()==0){
                printf("\033[92m   [Distributed child => HPC overshadow => time=%llu ms]\033[0m\n",
                       (unsigned long long)os_time());
                usleep(200000);
            }
            os_run_hpc_overshadow();
            _exit(0);
        } else if (c > 0) {
            /* parent just moves on to spawn the next or wait. */
        }
    }
    /* Wait for them all. In real code we'd track pids, but let's do a simple approach. */
    for(int i=0; i<2; i++) {
        wait(NULL);
    }

    printf("\033[96m╔══════════════════════════════════════════════╗\n");
    printf("║           DISTRIBUTED BLOCK END              ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
