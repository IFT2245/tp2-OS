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
#include <errno.h>

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
    return (uint64_t)ts.tv_sec * 1000ULL + (ts.tv_nsec / 1000000ULL);
}

/*
  A small helper that runs a CPU-bound loop in chunks:
   - totalIters: total loop iterations
   - chunkSize:  number of iterations per chunk
   - partialAcc: pointer to a 'long' accumulator
   The concurrency-stop flag is always checked once per chunk.
*/
static void os_hpc_cpu_loop_internal(long totalIters, long chunkSize, long *partialAcc) {
    long localSum = 0;
    long done = 0;
    while (done < totalIters) {
        /* If user requested concurrency stop, break early. */
        if (os_concurrency_stop_requested()) {
            break;
        }
        /*
          Process from 'done' to 'end - 1', or until totalIters is reached.
          Checking the stop flag once per chunk significantly reduces overhead
          compared to checking it in every iteration.
        */
        long end = done + chunkSize;
        if (end > totalIters) {
            end = totalIters;
        }
        for (long i = done; i < end; i++) {
            /* trivial CPU-bound random math */
            localSum += (i % 17) + (i % 11);
        }
        done = end;
    }
    *partialAcc = localSum;
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

    /* Capture the start time. */
    g_start_ms = now_ms();

    /* Reset ephemeral container count/paths. */
    g_container_count = 0;
    memset(g_container_paths, 0, sizeof(g_container_paths));

    /*
      We seed the random generator once, so HPC overshadow/overlay won't
      keep re-seeding. This helps produce more varied results if multiple HPC
      calls happen back-to-back.
    */
    srand((unsigned int)time(NULL));
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
            int rc = rmdir(path);
            if (rc == 0) {
                printf("\033[96m[-] Container removed (cleanup): %s\n\033[0m", path);
            } else {
                /* If removal fails, we print a warning. */
                fprintf(stderr, "\033[91m[!] Failed to remove container %s: %s\n\033[0m",
                        path, strerror(errno));
            }
            memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
            stats_inc_containers_removed();
        }
    }
}

uint64_t os_time(void) {
    /* Return real-world elapsed ms since os_init(). */
    const uint64_t now = now_ms();
    if (now < g_start_ms) {
        /* If clock jumps backward, clamp to zero. */
        return 0ULL;
    }
    return (now - g_start_ms);
}

/* Minimal logging with a small delay in normal mode only. */
void os_log(const char* msg) {
    if (!msg) return;
    if (stats_get_speed_mode() == 0) {
        printf("%s\n", msg);
        /* small user-friendly delay in normal mode */
        usleep(500000);
    }
}

/* ----------------------------------------------------------------
   Container creation
   ----------------------------------------------------------------
*/
void os_create_ephemeral_container(void) {
    if (g_container_count >= 32) return;

    char tmpl[] = "/tmp/os_cont_XXXXXX";
    if (mkdtemp(tmpl)) {
        strncpy(g_container_paths[g_container_count], tmpl, 255);
        g_container_count++;
        printf("\033[96m[+] Container created: %s (count=%d)\n\033[0m",
               tmpl, g_container_count);
        stats_inc_containers_created();
    } else {
        fprintf(stderr, "\033[91m[!] Failed to create ephemeral container: %s\n\033[0m",
                strerror(errno));
    }
}

/* Container removal (explicit). */
void os_remove_ephemeral_container(void) {
    if (g_container_count <= 0) return;
    g_container_count--;
    const char* path = g_container_paths[g_container_count];
    if (path[0]) {
        int rc = rmdir(path);
        if (rc == 0) {
            printf("\033[96m[-] Container removed: %s (remaining=%d)\n\033[0m",
                   path, g_container_count);
        } else {
            fprintf(stderr, "\033[91m[!] Failed to remove container %s: %s\n\033[0m",
                    path, strerror(errno));
        }
        memset(g_container_paths[g_container_count], 0, sizeof(g_container_paths[g_container_count]));
        stats_inc_containers_removed();
    }
}

/*
   HPC overshadow => spawns multiple CPU-bound threads (4..8).
   We'll do a chunked approach in each thread to check concurrency stop
   without too much overhead in single-step checking.
*/
static void* overshadow_thread(void* arg) {
    long *ret = (long*)arg;
    long sum = 0;

    /* We choose 7 million total iters, chunk size ~100k to reduce overhead. */
    os_hpc_cpu_loop_internal(7000000L, 100000L, &sum);

    *ret = sum;
    return NULL;
}

void os_run_hpc_overshadow(void) {
    if (os_concurrency_stop_requested()) {
        printf("\033[91m[OS] concurrency stop => HPC overshadow aborted.\n\033[0m");
        return;
    }

    /* pick a random number of CPU-bound threads from 4..8. */
    int n = 4 + (rand() % 5); /* range = [4..8]. */

    long* results = (long*)calloc((size_t)n, sizeof(long));
    pthread_t* th = (pthread_t*)malloc((size_t)n*sizeof(pthread_t));
    if (!results || !th) {
        fprintf(stderr, "\033[91m[!] HPC overshadow => allocation failed.\n\033[0m");
        free(results);
        free(th);
        return;
    }

    if (stats_get_speed_mode() == 0) {
        printf("\033[92m[HPC Overshadow] => spawning %d threads\n\033[0m", n);
        usleep(200000);
    }

    for (int i = 0; i < n; i++) {
        int rc = pthread_create(&th[i], NULL, overshadow_thread, &results[i]);
        if (rc != 0) {
            fprintf(stderr, "\033[91m[!] HPC overshadow => pthread_create failed: %s\n\033[0m",
                    strerror(rc));
            /* On error, we won't spawn the rest. */
            n = i;
            break;
        }
        if (stats_get_speed_mode() == 0) {
            printf("\033[92m   HPC Overshadow Thread #%d => time=%llu ms => started.\n\033[0m",
                   i+1, (unsigned long long)os_time());
            usleep(150000);
        }
    }

    /* join them */
    for (int i = 0; i < n; i++) {
        pthread_join(th[i], NULL);
        if (stats_get_speed_mode() == 0) {
            printf("\033[92m   HPC Overshadow Thread #%d => time=%llu ms => finished.\n\033[0m",
                   i+1, (unsigned long long)os_time());
            usleep(100000);
        }
    }

    free(th);
    free(results);

    if (stats_get_speed_mode() == 0) {
        printf("\033[92m   HPC Overshadow => time=%llu ms => END.\n\033[0m",
               (unsigned long long)os_time());
    }
}

/*
   HPC overlay => spawns a smaller random number of CPU-bound threads [2..4],
   with a shorter loop (4 million).
*/
static void* overlay_thread(void* arg) {
    long *ret = (long*)arg;
    long sum = 0;

    /* We'll do 4 million total, chunk size 100k. */
    os_hpc_cpu_loop_internal(4000000L, 100000L, &sum);

    *ret = sum;
    return NULL;
}

void os_run_hpc_overlay(void) {
    if (os_concurrency_stop_requested()) {
        printf("\033[91m[OS] concurrency stop => HPC overlay aborted.\n\033[0m");
        return;
    }

    /* random number of threads in [2..4] */
    int n = 2 + (rand() % 3); /* range=[2..4]. */

    long* results = (long*)calloc((size_t)n, sizeof(long));
    pthread_t* th = (pthread_t*)malloc((size_t)n*sizeof(pthread_t));
    if (!results || !th) {
        fprintf(stderr, "\033[91m[!] HPC overlay => allocation failed.\n\033[0m");
        free(results);
        free(th);
        return;
    }

    if (stats_get_speed_mode() == 0) {
        printf("\033[92m[HPC Overlay] => spawning %d threads\n\033[0m", n);
        usleep(200000);
    }

    for (int i = 0; i < n; i++) {
        int rc = pthread_create(&th[i], NULL, overlay_thread, &results[i]);
        if (rc != 0) {
            fprintf(stderr, "\033[91m[!] HPC overlay => pthread_create failed: %s\n\033[0m",
                    strerror(rc));
            n = i;
            break;
        }
        if (stats_get_speed_mode() == 0) {
            printf("\033[92m   HPC Overlay Thread #%d => time=%llu ms => started.\n\033[0m",
                   i+1, (unsigned long long)os_time());
            usleep(150000);
        }
    }

    /* join them */
    for (int i=0; i<n; i++) {
        pthread_join(th[i], NULL);
        if (stats_get_speed_mode()==0) {
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
   Pipeline example => multi-stage demonstration:
   - stage1 child does some "work"
   - passes data through pipe
   - stage2 child uses the data.
*/
void os_pipeline_example(void) {
    printf("\033[95m╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");

    if (os_concurrency_stop_requested()) {
        printf("\033[91m[OS] concurrency stop => skipping pipeline.\n\033[0m");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        fprintf(stderr, "\033[91m[!] pipeline => pipe() failed: %s\n\033[0m", strerror(errno));
        return;
    }

    pid_t c1 = fork();
    if (c1 < 0) {
        fprintf(stderr, "\033[91m[!] pipeline => fork() failed: %s\n\033[0m", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    if (c1 == 0) {
        /* Child1 => stage #1. */
        close(pipefd[0]);
        if (stats_get_speed_mode()==0) {
            printf("\033[92m   [Pipeline child1 => start => time=%llu ms]\033[0m\n",
                   (unsigned long long)os_time());
            usleep(100000);
        }
        /* Write some data to pass to next stage. */
        const char* data="Hello from stage1";
        write(pipefd[1], data, strlen(data)+1);
        close(pipefd[1]);
        if (stats_get_speed_mode()==0) {
            printf("\033[92m   [Pipeline child1 => end => time=%llu ms]\033[0m\n",
                   (unsigned long long)os_time());
        }
        _exit(0);
    } else {
        /* Parent => spawn stage2. */
        pid_t c2 = fork();
        if (c2 < 0) {
            fprintf(stderr, "\033[91m[!] pipeline => fork()2 failed: %s\n\033[0m", strerror(errno));
            kill(c1, SIGKILL);
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(c1, NULL, 0);
            return;
        }
        if (c2 == 0) {
            /* Child2 => read data from stage1. */
            close(pipefd[1]);
            char buf[128];
            memset(buf,0,sizeof(buf));
            read(pipefd[0], buf, sizeof(buf)-1);
            if (stats_get_speed_mode()==0) {
                printf("\033[92m   [Pipeline child2 => got data=\"%s\" => time=%llu ms]\033[0m\n",
                       buf, (unsigned long long)os_time());
                usleep(150000);
            }
            close(pipefd[0]);
            _exit(0);
        } else {
            /* Parent => close pipe, wait for both children. */
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(c1, NULL, 0);
            waitpid(c2, NULL, 0);
        }
    }

    printf("\033[96m╔══════════════════════════════════════════════╗\n");
    printf("║             PIPELINE BLOCK END               ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");

    if (stats_get_speed_mode()==0) {
        usleep(200000);
    }
}

/*
   Distributed example => fork multiple HPC overshadow tasks in parallel
   to simulate "remote nodes".
*/
void os_run_distributed_example(void) {
    printf("\033[95m╔══════════════════════════════════════════════╗\n");
    printf("║          DISTRIBUTED BLOCK START             ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");

    if (os_concurrency_stop_requested()) {
        printf("\033[93m[OS] concurrency stop => skipping distributed.\n\033[0m");
        return;
    }

    /* We'll spawn 2 HPC overshadow children. */
    for (int i=0; i<2; i++) {
        if (os_concurrency_stop_requested()) {
            break;
        }
        pid_t c = fork();
        if (c < 0) {
            fprintf(stderr, "\033[91m[!] distributed => fork failed: %s\n\033[0m",
                    strerror(errno));
            break;
        }
        if (c == 0) {
            /* child => HPC overshadow. */
            if(stats_get_speed_mode()==0) {
                printf("\033[92m   [Distributed child => HPC overshadow => time=%llu ms]\033[0m\n",
                       (unsigned long long)os_time());
                usleep(200000);
            }
            os_run_hpc_overshadow();
            _exit(0);
        }
    }

    /* Wait for them all. In real code we'd track pids more carefully. */
    for (int i=0; i<2; i++) {
        wait(NULL);
    }

    printf("\033[96m╔══════════════════════════════════════════════╗\n");
    printf("║           DISTRIBUTED BLOCK END              ║\n");
    printf("╚══════════════════════════════════════════════╝\033[0m\n");
}
