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

/* Colors for nice logs */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_CYAN    "\033[96m"
#define CLR_GREEN   "\033[92m"
#define CLR_YELLOW  "\033[93m"

/* Speed-friendly sleep function => scale by speed mode. */
static void sim_sleep(unsigned int us) {
    int sm = stats_get_speed_mode();
    if(sm == 1) {
        usleep(us / 10 + 1);
    } else {
        usleep(us);
    }
}

/* Concurrency stop flag stored here, set by set_os_concurrency_stop_flag(...). */
static volatile sig_atomic_t g_concurrency_stop_flag = 0;

void set_os_concurrency_stop_flag(int val) {
    g_concurrency_stop_flag = (sig_atomic_t)val;
}

int os_concurrency_stop_requested(void) {
    return (int)g_concurrency_stop_flag;
}

/* Time reference. */
static uint64_t g_start_ms = 0;

/* Container management */
static int       g_container_count = 0;
static char      g_container_paths[32][256]; /* up to 32 ephemeral containers */

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec)*1000ULL + (ts.tv_nsec / 1000000ULL);
}

void os_init(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    g_start_ms = now_ms();
    g_container_count = 0;
    memset(g_container_paths, 0, sizeof(g_container_paths));

    printf(CLR_BOLD CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf("║               OS INIT COMPLETE               ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(250000);
}

void os_cleanup(void) {
    /* remove any leftover ephemeral containers in reverse order */
    while (g_container_count > 0) {
        g_container_count--;
        const char* path = g_container_paths[g_container_count];
        if (path && path[0]) {
            rmdir(path);
            memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
            printf(CLR_CYAN "[-] Container removed (cleanup): %s\n" CLR_RESET, path);
            sim_sleep(200000);
            stats_inc_containers_removed();
        }
    }
    printf(CLR_BOLD CLR_MAGENTA "╔══════════════════════════════════════════════╗\n");
    printf(               "║             OS CLEANUP COMPLETE             ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(250000);
}

uint64_t os_time(void) {
    uint64_t now = now_ms();
    if (now < g_start_ms) return 0ULL;
    return (now - g_start_ms);
}

void os_log(const char* msg) {
    if (msg) {
        printf("%s\n", msg);
        sim_sleep(150000);
    }
}

void os_create_ephemeral_container(void) {
    if (g_container_count >= 32) return;
    char tmpl[] = "/tmp/os_cont_XXXXXX";
    if (mkdtemp(tmpl)) {
        strncpy(g_container_paths[g_container_count], tmpl, 255);
        g_container_count++;
        printf(CLR_CYAN "[+] Container created: %s (count=%d)\n" CLR_RESET,
               tmpl, g_container_count);
        sim_sleep(250000);
        stats_inc_containers_created();
    }
}

void os_remove_ephemeral_container(void) {
    if (g_container_count <= 0) return;
    g_container_count--;
    const char* path = g_container_paths[g_container_count];
    if (path[0]) {
        rmdir(path);
        memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
        printf(CLR_CYAN "[-] Container removed: %s (remaining=%d)\n" CLR_RESET,
               path, g_container_count);
        sim_sleep(250000);
        stats_inc_containers_removed();
    }
}

/* HPC overshadow thread => CPU-bound workload. */
static void* overshadow_thread(void* arg) {
    long *ret = (long*)arg;
    long sum = 0;
    for (long i=0; i<700000; i++) {
        sum += (i % 17) + (i % 11);
    }
    *ret = sum;
    return NULL;
}

void os_run_hpc_overshadow(void) {
    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║      HPC-OVERSHADOW BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(200000);

    int n=4;
    long* results = (long*)calloc(n, sizeof(long));
    pthread_t* th = (pthread_t*)malloc(n*sizeof(pthread_t));

    for (int i=0; i<n; i++) {
        pthread_create(&th[i], NULL, overshadow_thread, &results[i]);
        printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => started.\n" CLR_RESET,
               i+1, (unsigned long long)os_time());
        sim_sleep(300000);
    }
    for (int i=0; i<n; i++) {
        pthread_join(th[i], NULL);
        printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => finished.\n" CLR_RESET,
               i+1, (unsigned long long)os_time());
        sim_sleep(300000);
    }

    free(th);
    free(results);

    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║       HPC-OVERSHADOW BLOCK END              ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(200000);
    printf("HPC overshadow done\n");
    sim_sleep(200000);
}

void os_pipeline_example(void) {
    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK START            ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(200000);

    pid_t c = fork();
    if (c == 0) {
        printf(CLR_GREEN "   [Pipeline child => started => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        sim_sleep(50000);
        printf(CLR_GREEN "   [Pipeline child => finishing => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        _exit(0);
    } else if (c > 0) {
        printf(CLR_GREEN "   [Pipeline parent => waiting child => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        sim_sleep(200000);
        waitpid(c, NULL, 0);
    }

    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK END              ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(200000);
    printf("Pipeline end\n");
    sim_sleep(200000);
}

void os_run_distributed_example(void) {
    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║          DISTRIBUTED BLOCK START            ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(200000);

    pid_t c = fork();
    if (c == 0) {
        printf(CLR_GREEN "   [Distributed child => HPC overshadow => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        sim_sleep(200000);
        os_run_hpc_overshadow();
        _exit(0);
    } else if (c > 0) {
        printf(CLR_GREEN "   [Distributed parent => waiting => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        sim_sleep(200000);
        waitpid(c, NULL, 0);
    }

    printf(CLR_CYAN "╔══════════════════════════════════════════════╗\n");
    printf("║           DISTRIBUTED BLOCK END             ║\n");
    printf("╚══════════════════════════════════════════════╝" CLR_RESET "\n");
    sim_sleep(200000);
}
