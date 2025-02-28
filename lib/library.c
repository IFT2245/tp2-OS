#include "library.h"
#include "log.h"
#include "scoreboard.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

/* -------------------------------------------------------------------
   (A) Global flags for skipping or partial "look" at tests
   ------------------------------------------------------------------- */

static volatile sig_atomic_t g_skip_remaining_tests = 0;
static volatile sig_atomic_t g_look_remaining_tests = 0;

int skip_remaining_tests_requested(void) {
    return (g_skip_remaining_tests != 0);
}
void set_skip_remaining_tests(const int val) {
    g_skip_remaining_tests = (val ? 1 : 0);
}

int look_remaining_tests_requested(void) {
    return (g_look_remaining_tests != 0);
}
void set_look_remaining_tests(const int val) {
    g_look_remaining_tests = (val ? 1 : 0);
}

/* Tracks slow-mode on/off globally: */
static int g_slowMode = 0;

void set_slow_mode(const int onOff) {
    g_slowMode = (onOff != 0);
}
int is_slow_mode(void) {
    return g_slowMode;
}

/**
 * @brief handle_signal for SIGINT, SIGTERM
 */
void handle_signal(const int signum){
    scoreboard_save();
    fflush(stdout);
    fflush(stderr);

    if(signum == SIGINT) {
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
   (B) Immediate preemption setup: sigaltstack + setitimer + siglongjmp
   ------------------------------------------------------------------- */

#define MAX_CORES 64

static sigjmp_buf g_jmpbufs[MAX_CORES];
static int        g_registered[MAX_CORES] = {0};

static __thread int t_core_id = -1;

static stack_t g_altstack;

void preempt_signal_handler(int sig) {
    if(sig == SIGALRM){
        int cid = t_core_id;
        if(cid >= 0 && cid < MAX_CORES && g_registered[cid]){
            siglongjmp(g_jmpbufs[cid], 1);
        }
    }
}

void init_preempt_timer(void) {
    // 1) Alternate stack
    memset(&g_altstack, 0, sizeof(g_altstack));
    g_altstack.ss_sp = malloc(SIGSTKSZ);
    if(!g_altstack.ss_sp){
        log_error("init_preempt_timer => cannot allocate altstack");
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

    // 2) sigaction
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = preempt_signal_handler;
    sa.sa_flags   = SA_ONSTACK | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGALRM, &sa, NULL) < 0){
        log_error("init_preempt_timer => sigaction fail: %s", strerror(errno));
        return;
    }

    // 3) block SIGALRM in main so threads inherit blocked
    sigset_t blockSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &blockSet, NULL);

    // 4) setitimer => 1ms
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

void disable_preempt_timer(void){
    struct itimerval it;
    memset(&it, 0, sizeof(it));
    setitimer(ITIMER_REAL, &it, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGALRM, &sa, NULL);

    if(g_altstack.ss_sp){
        free(g_altstack.ss_sp);
        g_altstack.ss_sp = NULL;
        g_altstack.ss_size = 0;
    }
    log_info("disable_preempt_timer => timer off + altstack freed");
}

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
