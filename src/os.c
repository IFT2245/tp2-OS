#include "os.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_CYAN    "\033[96m"
#define CLR_GREEN   "\033[92m"
#define CLR_YELLOW  "\033[93m"

static uint64_t g_start_ms = 0;
static int       g_container_count = 0;
static char      g_container_paths[32][256]; /* up to 32 ephemeral containers */

static uint64_t now_ms(void) {
    /* Real-time measurement for user-friendly ASCII logs, not for internal sim_time. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec) * 1000ULL + (ts.tv_nsec / 1000000ULL);
}

void os_init(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    g_start_ms = now_ms();
    g_container_count = 0;
    memset(g_container_paths, 0, sizeof(g_container_paths));

    printf(CLR_BOLD CLR_MAGENTA "==== OS INIT ====\n" CLR_RESET);
    /* small delay to see output distinctly */
    usleep(250000);
}

void os_cleanup(void) {
    /* remove any leftover ephemeral containers in reverse order */
    while (g_container_count > 0) {
        g_container_count--;
        const char* path = g_container_paths[g_container_count];
        if (path && path[0]) {
            rmdir(path);
            memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
            printf(CLR_CYAN "$$$$$ Container removed: %s (cleanup) $$$$$\n" CLR_RESET, path);
            usleep(200000);
        }
    }
    printf(CLR_BOLD CLR_MAGENTA "==== OS CLEANUP ====\n" CLR_RESET);
    usleep(250000);
}

uint64_t os_time(void) {
    /* Return how many ms have elapsed since os_init(), purely for user display logs. */
    uint64_t now = now_ms();
    if (now < g_start_ms) return 0ULL;
    return (now - g_start_ms);
}

void os_log(const char* msg) {
    if (msg) {
        printf("%s\n", msg);
        usleep(150000);
    }
}

void os_create_ephemeral_container(void) {
    if (g_container_count >= 32) return;
    char tmpl[] = "/tmp/os_cont_XXXXXX";
    if (mkdtemp(tmpl)) {
        strncpy(g_container_paths[g_container_count], tmpl, 255);
        g_container_count++;
        printf(CLR_CYAN "$$$$$ Container created: %s (count=%d) $$$$$\n" CLR_RESET,
               tmpl, g_container_count);
        usleep(250000);
    }
}

void os_remove_ephemeral_container(void) {
    if (g_container_count <= 0) return;
    g_container_count--;
    const char* path = g_container_paths[g_container_count];
    if (path[0]) {
        rmdir(path);
        memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
        printf(CLR_CYAN "$$$$$ Container removed: %s (remaining=%d) $$$$$\n" CLR_RESET,
               path, g_container_count);
        usleep(250000);
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
    printf(CLR_CYAN "$$$$$$$$$$$$$ HPC-OVERSHADOW BLOCK START $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);

    int n=4;
    long* results = (long*)calloc(n, sizeof(long));
    pthread_t* th = (pthread_t*)malloc(n * sizeof(pthread_t));

    for (int i=0; i<n; i++) {
        pthread_create(&th[i], NULL, overshadow_thread, &results[i]);
        printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => started.\n" CLR_RESET,
               i+1, (unsigned long long)os_time());
        usleep(300000);
    }
    for (int i=0; i<n; i++) {
        pthread_join(th[i], NULL);
        printf(CLR_GREEN "   HPC Overshadow Thread #%d => time=%llu ms => finished.\n" CLR_RESET,
               i+1, (unsigned long long)os_time());
        usleep(300000);
    }

    free(th);
    free(results);

    printf(CLR_CYAN "$$$$$$$$$$$$$ HPC-OVERSHADOW BLOCK END $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);
    printf("HPC overshadow done\n");
    usleep(200000);
}

void os_pipeline_example(void) {
    printf("Pipeline start\n");
    printf(CLR_CYAN "$$$$$$$$$$$ PIPELINE BLOCK START $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);

    pid_t c = fork();
    if (c == 0) {
        /* child */
        printf(CLR_GREEN "   [Pipeline child => started => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        usleep(50000);
        printf(CLR_GREEN "   [Pipeline child => finishing => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        _exit(0);
    } else if (c > 0) {
        /* parent */
        printf(CLR_GREEN "   [Pipeline parent => waiting child => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        usleep(200000);
        waitpid(c, NULL, 0);
    }

    printf(CLR_CYAN "$$$$$$$$$$$ PIPELINE BLOCK END $$$$$$$$$$$$$\n" CLR_RESET);
    usleep(200000);
    printf("Pipeline end\n");
    usleep(200000);
}

void os_run_distributed_example(void) {
    printf("Distributed example: fork\n");
    printf(CLR_CYAN "$$$$$$$ DISTRIBUTED BLOCK START $$$$$$$$\n" CLR_RESET);
    usleep(200000);

    pid_t c = fork();
    if (c == 0) {
        /* child */
        printf(CLR_GREEN "   [Distributed child => HPC overshadow => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        usleep(200000);
        os_run_hpc_overshadow();
        _exit(0);
    } else if (c > 0) {
        printf(CLR_GREEN "   [Distributed parent => waiting => time=%llu ms]\n" CLR_RESET,
               (unsigned long long)os_time());
        usleep(200000);
        waitpid(c, NULL, 0);
    }

    printf(CLR_CYAN "$$$$$$$ DISTRIBUTED BLOCK END $$$$$$$$\n" CLR_RESET);
    usleep(200000);
}
