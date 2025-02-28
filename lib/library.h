#ifndef LIBRARY_H
#define LIBRARY_H

#include <signal.h>   // For SIGALRM, etc.
#include <setjmp.h>   // For siglongjmp, sigsetjmp

/* -------------------------------------------------------------------
   ------------------------------------------------------------------- */

/**
 * @brief If user hits SIGINT or SIGTERM, we do scoreboard saving, etc.
 */
void handle_signal(int signum);

/**
 * @brief Return 1 if we decided to skip remaining concurrency tests.
 */
int skip_remaining_tests_requested(void);

/**
 * @brief Set the skip_remaining_tests volatile flag.
 */
void set_skip_remaining_tests(const int val);

/**
 * @brief Return 1 if we only want to "look" at remaining tests but not actually do them.
 */
int look_remaining_tests_requested(void);

/**
 * @brief Turn on/off "look_remaining_tests" mode.
 */
void set_look_remaining_tests(const int val);


int is_bonus_test(void);
int is_slow_mode(void);
void set_bonus_test(const int onOff);
void set_slow_mode(const int onOff);


/* -------------------------------------------------------------------
/* -------------------------------------------------------------------
    ADVANCED PREEMPTION API
   ------------------------------------------------------------------- */

/**
 * @brief Initialize a periodic timer that fires SIGALRM every 1 ms,
 *        installing a special signal handler on an alternate stack.
 *        The handler calls siglongjmp immediately for “maximum immediacy.”
 *
 *        Also configures signal masks so that threads can choose
 *        when to block/unblock SIGALRM.
 */
void init_preempt_timer(void);

/**
 * @brief Disable the timer so no more SIGALRM arrives (and free altstack).
 */
void disable_preempt_timer(void);

/**
 * @brief The advanced signal handler that calls siglongjmp
 *        from within the interrupt.
 *        Do NOT call this yourself; it is set by init_preempt_timer().
 */
void preempt_signal_handler(int sig);

/**
 * @brief Assign a “core ID” to the current thread. Each scheduling
 *        thread (main core or HPC) must call this once at startup.
 */
void set_core_id_for_this_thread(int coreId);

/**
 * @brief Retrieve the current thread’s core ID (thread-local).
 */
int get_core_id_for_this_thread(void);

/**
 * @brief Register the thread’s sigjmp_buf so the preemption handler
 *        knows where to jump.
 *        Typically:
 *            sigjmp_buf env;
 *            if (sigsetjmp(env, 1) == 0) {
 *                register_jmpbuf_for_core(coreId, env);
 *                ...
 *            }
 */
void register_jmpbuf_for_core(int coreId, sigjmp_buf env);

/**
 * @brief Block SIGALRM in this thread (so we are safe from preemption).
 */
void block_preempt_signal(void);

/**
 * @brief Unblock SIGALRM in this thread (allowing immediate preemption).
 */
void unblock_preempt_signal(void);

#ifdef __cplusplus
}
#endif

#endif // LIBRARY_H
