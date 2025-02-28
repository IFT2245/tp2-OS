#include "library.h"
#include "../lib/log.h"
#include "../lib/scoreboard.h"  // for scoreboard_get_final_score, scoreboard_save, etc.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

/* -------------------------------------------------------------------
   (A) YOUR ORIGINAL GLOBALS & FLAG LOGIC
   ------------------------------------------------------------------- */

static volatile sig_atomic_t g_skip_remaining_tests = 0;
static volatile sig_atomic_t g_look_remaining_tests = 0;

/**
 * @brief Check if user wants to skip concurrency tests.
 */
int skip_remaining_tests_requested(void) {
    return (g_skip_remaining_tests != 0);
}
void set_skip_remaining_tests(const int val) {
    g_skip_remaining_tests = (val ? 1 : 0);
}

/**
 * @brief Check if user wants to only "look" at next tests (like partial).
 */
int look_remaining_tests_requested(void) {
    return (g_look_remaining_tests != 0);
}
void set_look_remaining_tests(const int val) {
    g_look_remaining_tests = (val ? 1 : 0);
}


/* Tracks slow-mode on/off globally. */
// This variable controls slow vs. fast sleep logic:
static int g_slowMode = 0;
static int g_bonusTest = 0;

void set_slow_mode(const int onOff) {
    // Just record the local preference, no shell calls:
    g_slowMode = (onOff != 0);
}

void set_bonus_test(const int onOff) {
    // *** ADDED FOR BONUS TEST ***
    g_bonusTest = (onOff != 0);
}
int is_slow_mode(void) {
    return g_slowMode;
}
int is_bonus_test(void) {
    // *** ADDED FOR BONUS TEST ***
    return g_bonusTest;
}


/**
 * @brief handle_signal for SIGINT, SIGTERM
 */
void handle_signal(const int signum){
    // Save scoreboard on any kill signal
    scoreboard_save();
    fflush(stdout);
    fflush(stderr);

    if(signum == SIGINT) {
        // Immediately exit with the final scoreboard code
        log_warn("Caught SIGINT => exiting now");
        exit(scoreboard_get_final_score());
    }
    if(signum == SIGTERM) {
        if (look_remaining_tests_requested()) {
            set_skip_remaining_tests(1);
            log_warn("Caught SIGTERM => skip concurrency tests next");
        } else {
            log_warn("Caught SIGTERM => exiting now");
            exit(scoreboard_get_final_score());
        }
    }
}

/* -------------------------------------------------------------------
   (B) ADVANCED PREEMPTION: JUMP BUFFERS + SIGNAL HANDLER
   ------------------------------------------------------------------- */

/// NEW CODE: maximum number of core threads
#define MAX_CORES 64

/// NEW CODE: one jump buffer per core ID
static sigjmp_buf g_jmpbufs[MAX_CORES];
static int        g_registered[MAX_CORES] = {0};

/// NEW CODE: thread-local “core ID”
static __thread int t_core_id = -1;

/// NEW CODE: For the alternate signal stack
static stack_t g_altstack;

/**
 * @brief The signal handler that calls siglongjmp from the interrupt.
 *        Must do minimal operations => only async-signal-safe calls!
 */
void preempt_signal_handler(int sig) {
    if(sig == SIGALRM) {
        int cid = t_core_id;
        if(cid >= 0 && cid < MAX_CORES && g_registered[cid]){
            // Jump out immediately => skip the rest of do_cpu_work
            siglongjmp(g_jmpbufs[cid], 1);
            // not reached
        }
    }
}

/**
 * @brief Initialize the 1ms timer + advanced preempt setup.
 *        - Installs an alternate stack for the signal handler
 *        - Installs the SIGALRM handler
 *        - Blocks SIGALRM by default in this (and child) threads
 *        - setitimer(ITIMER_REAL) to 1ms
 */
void init_preempt_timer(void) {
    /* 1) Setup alternate stack for signals (64KB). */
    memset(&g_altstack, 0, sizeof(g_altstack));
    g_altstack.ss_sp = malloc(SIGSTKSZ);
    if(!g_altstack.ss_sp){
        log_error("init_preempt_timer => cannot allocate altstack memory!");
        return;
    }
    g_altstack.ss_size = SIGSTKSZ;
    g_altstack.ss_flags= 0;

    if(sigaltstack(&g_altstack, NULL) < 0){
        log_error("init_preempt_timer => sigaltstack fail: %s", strerror(errno));
        free(g_altstack.ss_sp);
        g_altstack.ss_sp = NULL;
        return;
    }

    /* 2) Setup the signal action for SIGALRM. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = preempt_signal_handler;  // direct function
    sa.sa_flags   = SA_ONSTACK | SA_NODEFER; // altstack + allow nested
    sigemptyset(&sa.sa_mask);

    if(sigaction(SIGALRM, &sa, NULL) < 0){
        log_error("init_preempt_timer => sigaction(SIGALRM) fail: %s", strerror(errno));
        return;
    }

    /* 3) By default, block SIGALRM in *this* thread => inherited by future threads.
       We'll let each worker thread unblock it only during do_cpu_work. */
    sigset_t blockSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGALRM);
    if(pthread_sigmask(SIG_BLOCK, &blockSet, NULL) != 0){
        log_error("init_preempt_timer => pthread_sigmask block fail: %s", strerror(errno));
        return;
    }

    /* 4) Start the 1ms timer. */
    struct itimerval it;
    memset(&it, 0, sizeof(it));
    it.it_interval.tv_sec  = 0;
    it.it_interval.tv_usec = 1000; // 1ms
    it.it_value = it.it_interval;

    if(setitimer(ITIMER_REAL, &it, NULL) < 0){
        log_error("init_preempt_timer => setitimer fail: %s", strerror(errno));
        return;
    }

    log_info("init_preempt_timer => installed 1ms SIGALRM for immediate preemption");
}

/**
 * @brief Disable the 1ms timer and restore normal handler, plus free altstack.
 */
void disable_preempt_timer(void) {
    // 1) Stop the timer
    struct itimerval it;
    memset(&it, 0, sizeof(it));
    setitimer(ITIMER_REAL, &it, NULL);

    // 2) Restore default handler for SIGALRM
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGALRM, &sa, NULL);

    // 3) Release the altstack memory
    if(g_altstack.ss_sp){
        free(g_altstack.ss_sp);
        g_altstack.ss_sp = NULL;
        g_altstack.ss_size = 0;
    }

    log_info("disable_preempt_timer => timer off + altstack freed");
}

/* -------------------------------------------------------------------
   (C) PER-THREAD CORE ID + REGISTERING JMPBUF
   ------------------------------------------------------------------- */

void set_core_id_for_this_thread(int coreId){
    t_core_id = coreId;
}

int get_core_id_for_this_thread(void){
    return t_core_id;
}

void register_jmpbuf_for_core(int coreId, sigjmp_buf env){
    if(coreId < 0 || coreId >= MAX_CORES){
        log_error("register_jmpbuf_for_core => invalid coreId=%d", coreId);
        return;
    }
    memcpy(g_jmpbufs[coreId], env, sizeof(sigjmp_buf));
    g_registered[coreId] = 1;
}

/* -------------------------------------------------------------------
   (D) BLOCK/UNBLOCK SIGALRM
   ------------------------------------------------------------------- */

void block_preempt_signal(void){
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}

void unblock_preempt_signal(void){
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}
