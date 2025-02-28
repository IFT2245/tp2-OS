#include "library.h"
#include "scoreboard.h"
#include "log.h"
#include <stdlib.h>
#include <signal.h>

static volatile sig_atomic_t g_skip_remaining_tests = 0;

int skip_remaining_tests_requested(void) {
    return (g_skip_remaining_tests != 0);
}

void set_skip_remaining_tests(const int val) {
    g_skip_remaining_tests = (val ? 1 : 0);
}

static volatile sig_atomic_t g_look_remaining_tests = 0;

int look_remaining_tests_requested(void) {
    return (g_look_remaining_tests != 0);
}

void set_look_remaining_tests(const int val) {
    g_look_remaining_tests = (val ? 1 : 0);
}

void handle_signal(const int signum){
    scoreboard_save();
    fflush(stdout);
    fflush(stderr);
    if(signum == SIGINT) {
        /* SIGINT => exit immediately, but save scoreboard + stats. */
        log_warn("Caught Signal to exit => exiting");
        exit(scoreboard_get_final_score());
    }

    if(signum == SIGTERM) {
        if (look_remaining_tests_requested())
        {
            set_skip_remaining_tests(1); // volatile value make an action possible
            log_warn("Caught signal to stop next concurrency");
        } else
        {
            log_warn("Caught Signal to exit => exiting");
            exit(scoreboard_get_final_score());
        }

    }
}

// Usage : if (skip_remaining_tests_requested) do