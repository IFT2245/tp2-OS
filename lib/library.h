#ifndef LIBRARY_H
#define LIBRARY_H

#include <signal.h>
#include <setjmp.h>

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

int is_bonus_test(void);
int is_slow_mode(void);
void set_bonus_test(const int onOff);
void set_slow_mode(const int onOff);

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
