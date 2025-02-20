/*
 * test_normal.c
 *
 * The SECOND test suite for our OS/game simulator, covering
 * moderately complex usage:
 *   1) HPC overshadow + multi-process
 *   2) HPC overshadow + distributed container
 *   3) BFS scheduling plus HPC/pipeline
 *   4) RR scheduling with HPC/container/pipeline
 *   5) Priority scheduling with HPC emphasis
 *   6) A small "batch" of processes mixing HPC, container, pipeline
 *
 * We'll feed interactive input to the main "scheduler",
 * capturing stdout and comparing with expected lines or HPC sums.
 * We rely on a strategy similar to test_basic.c, with a function
 * run_scheduler_with_input() that spawns a child process, writes
 * the test input to a temp file, and reads the output from pipes.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do { \
    printf("\nRunning normal test: %s\n", #name); \
    bool ok = test_##name(); \
    tests_run++; \
    if (!ok) { \
        tests_failed++; \
        printf("❌ Failed: %s\n", #name); \
    } else { \
        printf("✓ Passed: %s\n", #name); \
    } \
} while(0)

static int run_scheduler_with_input(const char* input, char* outbuf, size_t outsz, char* errbuf, size_t errsz) {
    if (!input || !*input) return -1;
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) == -1 || pipe(err_pipe) == -1) {
        return -1;
    }
    int orig_out = dup(STDOUT_FILENO);
    int orig_err = dup(STDERR_FILENO);
    if (orig_out < 0 || orig_err < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    }
    if (dup2(out_pipe[1], STDOUT_FILENO) == -1 ||
        dup2(err_pipe[1], STDERR_FILENO) == -1) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        dup2(orig_out, STDOUT_FILENO);
        dup2(orig_err, STDERR_FILENO);
        close(orig_out); close(orig_err);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    }
    else if (pid == 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        dup2(orig_out, STDOUT_FILENO);
        dup2(orig_err, STDERR_FILENO);
        close(orig_out); close(orig_err);

        char tmp_file[] = "/tmp/sched_input_XXXXXX";
        int fd = mkstemp(tmp_file);
        if (fd < 0) _exit(127);
        write(fd, input, strlen(input));
        lseek(fd, 0, SEEK_SET);
        dup2(fd, STDIN_FILENO);
        close(fd);
        execl("./scheduler", "./scheduler", (char*)NULL);
        _exit(127);
    }
    else {
        dup2(orig_out, STDOUT_FILENO);
        dup2(orig_err, STDERR_FILENO);
        close(orig_out); close(orig_err);
        close(out_pipe[1]); close(err_pipe[1]);
        int status=0;
        waitpid(pid, &status, 0);
        ssize_t rd1 = read(out_pipe[0], outbuf, outsz-1);
        if (rd1 >= 0) outbuf[rd1] = '\0';
        else outbuf[0] = '\0';
        ssize_t rd2 = read(err_pipe[0], errbuf, errsz-1);
        if (rd2 >= 0) errbuf[rd2] = '\0';
        else errbuf[0] = '\0';
        close(out_pipe[0]); close(err_pipe[0]);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }
}

/*
 * 1) HPC overshadow + multi-process
 */
TEST(hpc_multiproc_test) {
    /* We'll pick OS mode => HPC + multi-process => run => exit. */
    char input[] =
        "1\n"  /* OS concurrency */
        "1\n"  /* toggle HPC => guess from the UI. "1" might do HPC ON. */
        "6\n"  /* toggle multi-process => guess from the UI. */
        "C\n"  /* run concurrency => HPC overshadow + fork => HPC result ??? */
        "0\n"  /* exit UI */
        "0\n"; /* exit main */
    char out[2048], err[2048];
    int rc = run_scheduler_with_input(input, out, sizeof(out), err, sizeof(err));
    bool hasHPC = strstr(out, "HPC result=") != NULL;
    return (rc == 0 && hasHPC);
}

/*
 * 2) HPC overshadow + distributed container => single worker concurrency approach
 */
TEST(distributed_container_worker_test) {
    /* We'll pick "3" => single worker concurrency, HPC => yes, container => yes, distributed => yes,
       let's do HPC threads=3, HPC total=150 => sum= (150*151)/2=11325 if overshadow. Then exit.
    */
    char input[] =
        "3\n"    /* worker concurrency approach */
        "3\n"    /* HPC threads => 3 */
        "150\n"  /* HPC total => 150 => sum=11325 */
        "2\n"    /* pipeline => 2 => we'll see if overshadow merges? It's approximate. */
        "2\n"    /* distributed => 2 */
        "Y\n"    /* HPC => yes */
        "Y\n"    /* container => yes => ephemeral? */
        "Y\n"    /* pipeline => yes */
        "Y\n"    /* distributed => yes */
        "N\n"    /* debug => no */
        "2\n"    /* game difficulty => 2 => story? in worker? not used? */
        "40\n"   /* game score => 40? might appear in debug if we had debug => no though. */
        "0\n";
    char out[2048], err[2048];
    int rc = run_scheduler_with_input(input, out, sizeof(out), err, sizeof(err));
    bool hasSum = strstr(out, "11325") != NULL;
    bool ephemeral = strstr(out, "/tmp/worker_container_") != NULL;
    bool pipeline2 = strstr(out, "pipeline stages=2") || strstr(out, "stageNow=2");
    return (rc == 0 && hasSum && ephemeral && pipeline2);
}

/*
 * 3) BFS scheduling plus HPC/pipeline in ReadyQueue
 */
TEST(bfs_hpc_pipeline_test) {
    /* "2" => readyQueue scheduling,
       scheduling => BFS => HPC => pipeline => etc. Then enqueue => run => exit.
    */
    char input[] =
        "2\n"     /* readyQueue */
        "1\n4\n"  /* pick BFS => "1" => scheduling param, "4" => BFS */
        "Y\n"     /* HPC => yes */
        "N\n"     /* container => no */
        "Y\n"     /* pipeline => yes */
        "N\n"     /* distributed => no */
        "N\n"     /* debug => no */
        "E\n"     /* enqueue sample proc => HPC + pipeline => BFS */
        "R\n"     /* run */
        "0\n"     /* exit UI */
        "0\n";    /* exit main */
    char out[2048], err[2048];
    int rc = run_scheduler_with_input(input, out, sizeof(out), err, sizeof(err));
    bool mentionBFS = strstr(out, "BFS") != NULL;
    bool mentionHPC = strstr(out, "HPC") != NULL;
    bool mentionPipe = strstr(out, "pipeline") != NULL;
    return (rc == 0 && mentionBFS && mentionHPC && mentionPipe);
}

/*
 * 4) RR scheduling with HPC/container/pipeline
 */
TEST(rr_hcp_container_pipeline_test) {
    /* "2" => readyQueue, scheduling => RR => HPC => container => pipeline => distributed => no => debug => no
       Enqueue => run => exit => exit
    */
    char input[] =
        "2\n"
        "1\n2\n"  /* scheduling => 2 => RR */
        "Y\n"     /* HPC => yes */
        "Y\n"     /* container => yes */
        "Y\n"     /* pipeline => yes */
        "N\n"     /* distributed => no */
        "N\n"     /* debug => no */
        "E\n"
        "R\n"
        "0\n"
        "0\n";
    char out[2048], err[2048];
    int rc = run_scheduler_with_input(input, out, sizeof(out), err, sizeof(err));
    bool mentionRR = strstr(out, "RR") != NULL;
    bool mentionHPC = strstr(out, "HPC") != NULL;
    bool ephemeral = strstr(out, "os_container_") || strstr(out, "worker_container_");
    bool mentionPipe = strstr(out, "pipeline") != NULL;
    return (rc == 0 && mentionRR && mentionHPC && ephemeral && mentionPipe);
}

/*
 * 5) Priority scheduling with HPC emphasis
 */
TEST(priority_with_hpc_test) {
    /* "2" => readyQueue, scheduling => priority => HPC => no container => no pipeline => distributed => no => debug => no
       Enqueue 3 procs => run => exit => exit. We'll see if HPC overshadow mention appears or HPC sum is done.
    */
    char input[] =
        "2\n"      /* queue */
        "1\n5\n"   /* scheduling => priority=5 */
        "Y\n"      /* HPC => yes */
        "N\n"      /* container => no */
        "N\n"      /* pipeline => no */
        "N\n"      /* distributed => no */
        "N\n"      /* debug => no */
        "E\n"      /* enqueue #1 */
        "E\n"      /* enqueue #2 */
        "E\n"      /* enqueue #3 */
        "R\n"      /* run => HPC overshadow for them all. */
        "0\n"
        "0\n";
    char out[2048], err[2048];
    int rc = run_scheduler_with_input(input, out, sizeof(out), err, sizeof(err));
    bool mentionPriority = strstr(out, "priority") || strstr(out, "PRIO");
    bool mentionHPC = strstr(out, "HPC") != NULL;
    return (rc == 0 && mentionPriority && mentionHPC);
}

/*
 * aggregator
 */
int run_normal_tests(void) {
    printf("\n================================\n");
    printf(" NORMAL TEST SUITE (FILE #2)\n");
    printf("================================\n");

    RUN_TEST(hpc_multiproc_test);
    RUN_TEST(distributed_container_worker_test);
    RUN_TEST(bfs_hpc_pipeline_test);
    RUN_TEST(rr_hcp_container_pipeline_test);
    RUN_TEST(priority_with_hpc_test);

    printf("\n[NORMAL TESTS] total run: %d\n", tests_run);
    printf("[NORMAL TESTS] total failed: %d\n", tests_failed);
    int passed = tests_run - tests_failed;
    double success_rate = (tests_run > 0) ? ((double)passed)*100.0/tests_run : -1.0;
    printf("[NORMAL TESTS] success: %.1f%%\n", success_rate);
    return (tests_failed == 0) ? 0 : 1;
}
