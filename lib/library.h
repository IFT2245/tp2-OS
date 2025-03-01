#ifndef LIBRARY_H
#define LIBRARY_H

#include <signal.h>
#include <setjmp.h>

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_RED     "\033[91m"
#define CLR_GREEN   "\033[92m"
#define CLR_GRAY    "\033[90m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"

/**
 * If user hits SIGINT or SIGTERM => scoreboard saving, etc.
 */
void handle_signal(int signum);

/**
 * Return 1 if we decided to skip remaining concurrency tests.
 */
int skip_remaining_tests_requested(void);
void set_skip_remaining_tests(const int val);

/**
 * Return 1 if we only want to "look" at tests but not actually do them.
 */
int look_remaining_tests_requested(void);
void set_look_remaining_tests(const int val);

/**
 * Return 1 if we want slow-mode concurrency.
 */
int is_bonus_test(void);
void set_bonus_test(const int onOff);

/* Preemption with setitimer + altstack + siglongjmp */
void init_preempt_timer(void);
void disable_preempt_timer(void);
void preempt_signal_handler(int sig);

void set_core_id_for_this_thread(int coreId);
int  get_core_id_for_this_thread(void);
void register_jmpbuf_for_core(int coreId, sigjmp_buf env);

void block_preempt_signal(void);
void unblock_preempt_signal(void);

#endif // LIBRARY_H
