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

void handle_signal(const int signum){
    scoreboard_save();
    if(signum == SIGINT) {
        /* SIGINT => exit immediately, but save scoreboard + stats. */
        log_warn("Caught Signal to exit => exiting");
        exit(1);
    }

    if(signum == SIGTERM) {
        set_skip_remaining_tests(1); // volatile value make an action possible
    }
    log_warn("Caught signal to return to main menu => navigation activated");
}

// Usage : if (skip_remaining_tests_requested)