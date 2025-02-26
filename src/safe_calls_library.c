#include "safe_calls_library.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

int parse_int_strtol(const char* in, int fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    long v = strtol(in, &e, 10);
    if(e==in || errno==ERANGE || v<INT_MIN || v>INT_MAX) return fb;
    return (int)v;
}

long parse_long_strtol(const char* in, long fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    long v = strtol(in, &e, 10);
    if(e==in || errno==ERANGE) return fb;
    return v;
}

float parse_float_strtof(const char* in, float fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    float v = strtof(in, &e);
    if(e==in || errno==ERANGE) return fb;
    return v;
}

double parse_double_strtod(const char* in, double fb) {
    if(!in || !*in) return fb;
    errno=0;
    char* e=NULL;
    double v = strtod(in, &e);
    if(e==in || errno==ERANGE) return fb;
    return v;
}

/* ----------------------------------------------------------------
   SIGNAL HANDLER
   ----------------------------------------------------------------
*/
void handle_signal(int signum) {
    if(signum == SIGINT) {
        /* SIGINT => exit immediately, but save scoreboard + stats. */
        stats_inc_signal_sigint();
        printf("\nCaught SIGINT => Exiting.\n");
        const int fs = scoreboard_get_final_score();
        cleanup_and_exit(fs);
    }
    else if(signum == SIGTERM) {
        /* SIGTERM => do NOT abort the test in progress; instead:
           - Stop concurrency if we are running HPC overshadow or external shell
           - Mark that we should skip future tests, returning to menu. */
        stats_inc_signal_sigterm();
        printf(CLR_RED "\nCaught SIGTERM => Trying to return to menu\n" CLR_RESET);

        /* For concurrency (shell or HPC overshadow) we do want to stop: */
        set_os_concurrency_stop_flag(1);

        /* For normal scheduling tests, we only skip *remaining* tests: */
        set_skip_remaining_tests(1);
    }
    else {
        /* For completeness, track any other signals. */
        stats_inc_signal_other();
    }
}

/* ----------------------------------------------------------------
   CLEANUP AND EXIT
   ----------------------------------------------------------------
*/
void cleanup_and_exit(int code) {
    os_cleanup();
    scoreboard_save();
    scoreboard_close();
    stats_print_summary();
    exit(code);
}
