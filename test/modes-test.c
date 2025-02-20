/*
 * test_modes_refactored.c
 *
 * The FIFTH test file, revised for clarity and correctness, focusing on
 * multi-mode synergy in the OS/game simulator. Each test has a more
 * descriptive name reflecting the scenario under test. We feed custom
 * user input to the compiled "scheduler" binary and capture stdout/stderr
 * using pipes. We then parse the output to confirm HPC overshadow, ephemeral
 * container creation, pipeline synergy, BFS/RR/priority scheduling, distributed
 * concurrency, debug logs, and game difficulty all function as expected.
 *
 * Compile with the rest of the project, or load dynamically. Then call
 * run_modes_tests_refactored() to execute these synergy tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do {                                 \
    printf("\nRunning synergy test: %s\n", #name);           \
    bool ok = test_##name();                                \
    tests_run++;                                            \
    if (!ok) {                                              \
        tests_failed++;                                     \
        printf("❌ FAILED: %s\n", #name);                    \
    } else {                                                \
        printf("✓ PASSED: %s\n", #name);                    \
    }                                                       \
} while(0)

/*
 * Helper to run the scheduler with given user input, capturing stdout/stderr.
 * On success, returns the child's exit code (often 0 if no error).
 * On early error, returns -1.
 */
static int run_scheduler(const char* input,
                         char* outbuf, size_t outsz,
                         char* errbuf, size_t errsz) {
    if (!input || !*input) return -1;

    int out_pipe[2];
    int err_pipe[2];
    if (pipe(out_pipe) == -1 || pipe(err_pipe) == -1) {
        return -1;
    }

    int original_stdout = dup(STDOUT_FILENO);
    int original_stderr = dup(STDERR_FILENO);
    if (original_stdout < 0 || original_stderr < 0) {
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
        dup2(original_stdout, STDOUT_FILENO);
        dup2(original_stderr, STDERR_FILENO);
        close(original_stdout);
        close(original_stderr);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    } else if (pid == 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        dup2(original_stdout, STDOUT_FILENO);
        dup2(original_stderr, STDERR_FILENO);
        close(original_stdout);
        close(original_stderr);

        char temp_infile[] = "/tmp/modes_test_input_XXXXXX";
        int fd = mkstemp(temp_infile);
        if (fd < 0) _exit(127);

        write(fd, input, strlen(input));
        lseek(fd, 0, SEEK_SET);
        dup2(fd, STDIN_FILENO);
        close(fd);
        execl("./scheduler", "./scheduler", (char*)NULL);
        _exit(127);
    } else {
        dup2(original_stdout, STDOUT_FILENO);
        dup2(original_stderr, STDERR_FILENO);
        close(original_stdout);
        close(original_stderr);
        close(out_pipe[1]);
        close(err_pipe[1]);
        int status=0;
        waitpid(pid, &status, 0);

        ssize_t out_read = read(out_pipe[0], outbuf, outsz - 1);
        if (out_read >= 0) outbuf[out_read] = '\0';
        else outbuf[0] = '\0';

        ssize_t err_read = read(err_pipe[0], errbuf, errsz - 1);
        if (err_read >= 0) errbuf[err_read] = '\0';
        else errbuf[0] = '\0';

        close(out_pipe[0]);
        close(err_pipe[0]);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }
}

/*
 * 1) HPC overshadow in single-worker concurrency with pipeline=2, container, debug, game=easy
 *    Expect HPC result=5050, ephemeral container mention, pipeline stage=2, debug logs, gameDiff=1 => easy, gameScore=10
 */
#define bool int
#define true 1
#define false 0
TEST(hpc_pipeline_container_debug_easy) {
    char input[] =
        "3\n"    /* single worker concurrency */
        "3\n"    /* HPC threads=3 => overshadow => sum=5050 if HPC total=100 */
        "100\n"  /* HPC total => 100 => sum=5050 */
        "2\n"    /* pipeline stages=2 */
        "0\n"    /* distributed=0 */
        "Y\n"    /* HPC => yes */
        "Y\n"    /* container => yes => ephemeral */
        "Y\n"    /* pipeline => yes => 2 stages => overshadow HPC partial or afterwards */
        "N\n"    /* distributed => no */
        "Y\n"    /* debug => yes => logs HPC overshadow, ephemeral, pipeline synergy, game? */
        "1\n"    /* game difficulty => 1 => easy => gameScore=10 */
        "10\n"   /* game score => 10 */
        "0\n";   /* exit scheduler */

    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool HPCres = (strstr(out, "5050") != NULL);
    bool ephemeral = (strstr(out, "worker_container_") != NULL);
    bool pipeline = (strstr(out, "stageNow=2") || strstr(out, "current_stage=2"));
    bool debug = (strstr(out, "[Worker Debug]") != NULL || strstr(out, "[OS Debug]") != NULL);
    bool gameDiff = (strstr(out, "gameDiff=1") != NULL);
    bool gameScore = (strstr(out, "gameScore=10") != NULL);
    return (rc == 0 && HPCres && ephemeral && pipeline && debug && gameDiff && gameScore);
}

/*
 * 2) HPC overshadow in BFS scheduling with container+pipeline+distributed+debug => readyQueue approach
 */
#define bool int
#define true 1
#define false 0
TEST(hpc_bfs_container_pipeline_distributed_debug) {
    char input[] =
        "2\n"      /* readyQueue */
        "1\n4\n"   /* scheduling => BFS=4 */
        "Y\n"      /* HPC => yes overshadow */
        "Y\n"      /* container => yes ephemeral */
        "Y\n"      /* pipeline => yes */
        "Y\n"      /* distributed => yes => multiple forks */
        "Y\n"      /* debug => yes => synergy logs */
        "E\n"      /* enqueue sample => HPC overshadow BFS pipeline distributed container debug */
        "R\n"
        "0\n"
        "0\n";
    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool BFS = (strstr(out, "BFS") != NULL);
    bool HPC = (strstr(out, "HPC") != NULL);
    bool ephemeral = (strstr(out, "container_") != NULL);
    bool pipeline = (strstr(out, "pipeline") != NULL);
    bool dist = (strstr(out, "fork") != NULL || strstr(out, "distributed") != NULL);
    bool debug = (strstr(out, "[Worker Debug]") != NULL || strstr(out, "[OS Debug]") != NULL);
    return (rc == 0 && BFS && HPC && ephemeral && pipeline && dist && debug);
}

/*
 * 3) HPC overshadow in RR scheduling with HPC+container => debug => multiple enqueues
 */
#define bool int
#define true 1
#define false 0
TEST(hpc_rr_multi_enqueues_debug) {
    char input[512];
    snprintf(input, sizeof(input),
        "2\n"
        "1\n2\n"     /* scheduling => 2 => RR */
        "Y\n"        /* HPC => yes overshadow */
        "Y\n"        /* container => yes ephemeral */
        "N\n"        /* pipeline => no */
        "N\n"        /* distributed => no */
        "Y\n"        /* debug => yes => overshadow logs ??? */
        "E\nE\nE\n"  /* enqueue 3 times => HPC overshadow for all => ephemeral container => debug logs */
        "R\n"
        "0\n"
        "0\n");
    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool HPC = (strstr(out, "HPC") != NULL);
    bool mentionRR = (strstr(out, "RR") != NULL || strstr(out, "quantum") != NULL);
    bool ephemeral = (strstr(out, "container_") != NULL);
    bool debug = (strstr(out, "[Worker Debug]") != NULL || strstr(out, "[OS Debug]") != NULL);
    return (rc == 0 && HPC && mentionRR && ephemeral && debug);
}

/*
 * 4) HPC overshadow + priority scheduling + ephemeral container + pipeline + debug => 2 enqueues
 */
#define bool int
#define true 1
#define false 0
TEST(hpc_priority_container_pipeline_debug) {
    char input[] =
        "2\n"
        "1\n5\n"    /* scheduling => priority=5 */
        "Y\n"       /* HPC => yes overshadow */
        "Y\n"       /* container => ephemeral */
        "Y\n"       /* pipeline => yes => synergy */
        "N\n"       /* distributed => no */
        "Y\n"       /* debug => yes */
        "E\nE\n"
        "R\n"
        "0\n"
        "0\n";
    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool HPC = (strstr(out, "HPC") != NULL);
    bool ephemeral = (strstr(out, "container_") != NULL);
    bool pipeline = (strstr(out, "pipeline") != NULL);
    bool debug = (strstr(out, "[Worker Debug]") != NULL || strstr(out, "[OS Debug]") != NULL);
    bool priority = (strstr(out, "priority") != NULL || strstr(out, "PRIO") != NULL);
    return (rc == 0 && HPC && ephemeral && pipeline && debug && priority);
}

/*
 * 5) HPC overshadow + pipeline=3 + container + distributed=2 + debug + game=story => single worker concurrency
 */
TEST(hpc_container_pipeline_distributed_debug_story) {
    char input[] =
        "3\n"
        "4\n"      /* HPC threads=4 => overshadow => HPC total default=100 => sum=5050? */
        "100\n"
        "3\n"      /* pipeline => 3 */
        "2\n"      /* distributed => 2 forks */
        "Y\n"      /* HPC => yes overshadow */
        "Y\n"      /* container => ephemeral */
        "Y\n"      /* pipeline => yes => 3 stages */
        "Y\n"      /* distributed => yes => 2 forks */
        "Y\n"      /* debug => yes => logs synergy */
        "2\n"      /* game diff => 2 => story => gameScore=20? if code is consistent */
        "20\n"
        "0\n";
    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool HPCres = (strstr(out, "5050") != NULL);
    bool ephemeral = (strstr(out, "worker_container_") != NULL);
    bool pipeline3 = (strstr(out, "stageNow=3") || strstr(out, "current_stage=3"));
    bool dist = (strstr(out, "fork") != NULL);
    bool debug = (strstr(out, "[Worker Debug]") != NULL);
    bool gameDiff = (strstr(out, "gameDiff=2") != NULL);
    bool gameScore = (strstr(out, "gameScore=20") != NULL);
    return (rc==0 && HPCres && ephemeral && pipeline3 && dist && debug && gameDiff && gameScore);
}

/*
 * 6) HPC overshadow + BFS + ephemeral container + pipeline + distributed=3 + debug => queue approach
 */
TEST(hpc_bfs_ephemeral_pipeline_dist_debug) {
    char input[] =
        "2\n"
        "1\n4\n"     /* BFS */
        "Y\n"        /* HPC => yes overshadow */
        "Y\n"        /* container => ephemeral */
        "Y\n"        /* pipeline => yes => synergy */
        "Y\n"        /* distributed => yes => multiple forks */
        "Y\n"        /* debug => yes => synergy logs */
        "E\n"
        "R\n"
        "0\n"
        "0\n";
    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool BFS = (strstr(out, "BFS") != NULL);
    bool HPC = (strstr(out, "HPC") != NULL);
    bool ephemeral = (strstr(out, "container_") != NULL);
    bool pipeline = (strstr(out, "pipeline") != NULL);
    bool dist = (strstr(out, "fork") != NULL);
    bool debug = (strstr(out, "[Worker Debug]") != NULL || strstr(out, "[OS Debug]") != NULL);
    return (rc==0 && BFS && HPC && ephemeral && pipeline && dist && debug);
}

/*
 * 7) HPC overshadow + RR + ephemeral container + pipeline=2 + distributed=2 + debug => single worker concurrency
 */
TEST(hpc_rr_ephemeral_pipeline_dist_debug) {
    char input[] =
        "3\n"
        "2\n"
        "100\n"
        "2\n"
        "2\n"
        "Y\n"  /* HPC => overshadow */
        "Y\n"  /* container => ephemeral */
        "Y\n"  /* pipeline => 2 stages */
        "Y\n"  /* distributed => 2 forks */
        "Y\n"  /* debug => synergy logs */
        "3\n"  /* game diff => 3 => challenge => gameScore=30? We'll see. */
        "30\n"
        "0\n";
    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool HPCres = (strstr(out, "5050") != NULL);
    bool ephemeral = (strstr(out, "worker_container_") != NULL);
    bool pipeline2 = (strstr(out, "stageNow=2") || strstr(out, "current_stage=2"));
    bool dist = (strstr(out, "fork") != NULL);
    bool debug = (strstr(out, "[Worker Debug]") != NULL);
    bool gameDiff = (strstr(out, "gameDiff=3") != NULL);
    bool gameScore = (strstr(out, "gameScore=30") != NULL);
    return (rc==0 && HPCres && ephemeral && pipeline2 && dist && debug && gameDiff && gameScore);
}

/*
 * 8) HPC overshadow + priority + ephemeral container + pipeline=3 + distributed=3 + debug => queue approach,
 *    plus hypothetical game=survival => difficulty=5 => score=50. We'll do partial checks if code logs it.
 */
TEST(hpc_priority_container_pipeline_dist_debug_survival) {
    char input[] =
        "2\n"
        "1\n5\n"  /* priority scheduling=5 */
        "Y\n"     /* HPC => overshadow */
        "Y\n"     /* container => ephemeral */
        "Y\n"     /* pipeline => yes => 3??? not direct */
        "Y\n"     /* distributed => yes => 3??? not direct but synergy */
        "Y\n"     /* debug => synergy logs */
        /* no direct game approach in queue config, so we skip. Then enqueue => run => exit => exit. */
        "E\nE\n"
        "R\n"
        "0\n"
        "0\n";
    char out[4096], err[4096];
    int rc = run_scheduler(input, out, sizeof(out), err, sizeof(err));
    bool HPC = (strstr(out, "HPC") != NULL);
    bool priority = (strstr(out, "priority") != NULL || strstr(out, "PRIO") != NULL);
    bool ephemeral = (strstr(out, "container_") != NULL);
    bool pipeline = (strstr(out, "pipeline") != NULL);
    bool dist = (strstr(out, "fork") != NULL);
    bool debug = (strstr(out, "[Worker Debug]") != NULL || strstr(out, "[OS Debug]") != NULL);
    /* game=survival => difficulty=5 => score=50 => might not appear from queue config alone. We skip. */
    return (rc==0 && HPC && priority && ephemeral && pipeline && dist && debug);
}

/*
 * aggregator
 */
int run_modes_tests(void) {
    printf("\n==============================================\n");
    printf(" MODES TEST SUITE (FILE #5 - REFACTORED)\n");
    printf("==============================================\n");

    RUN_TEST(hpc_pipeline_container_debug_easy);
    RUN_TEST(hpc_bfs_container_pipeline_distributed_debug);
    RUN_TEST(hpc_rr_multi_enqueues_debug);
    RUN_TEST(hpc_priority_container_pipeline_debug);
    RUN_TEST(hpc_container_pipeline_distributed_debug_story);
    RUN_TEST(hpc_bfs_ephemeral_pipeline_dist_debug);
    RUN_TEST(hpc_rr_ephemeral_pipeline_dist_debug);
    RUN_TEST(hpc_priority_container_pipeline_dist_debug_survival);

    printf("\n[MODES TESTS REFACTORED] total run: %d\n", tests_run);
    printf("[MODES TESTS REFACTORED] total failed: %d\n", tests_failed);
    int passed = tests_run - tests_failed;
    double rate = (tests_run > 0) ? (double)passed*100.0 / tests_run : -1.0;
    printf("[MODES TESTS REFACTORED] success: %.1f%%\n", rate);
    return (tests_failed == 0) ? 0 : 1;
}

