#include "worker.h"
#include "../lib/log.h"
#include "../lib/scoreboard.h"
#include "../lib/library.h"
#include "../test/basic-tests.h"  // Now includes our expanded tests

#include <stdlib.h>
#include <signal.h>

/**
 * We run all refined tests, show the scoreboard, and exit with final score.
 */
static void do_run_tests(void){
    set_log_level(LOG_LEVEL_INFO);
    run_all_tests();       // The refined test suite (includes multi-tests)
    show_scoreboard();
    scoreboard_save();
    fflush(stdout);
    fflush(stderr);
}

int main(void){
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // 1) Install the advanced preempt timer
    init_preempt_timer();

    scoreboard_load();
    do_run_tests();

    // 2) Turn off timer + free altstack
    disable_preempt_timer();

    return scoreboard_get_final_score();
}
