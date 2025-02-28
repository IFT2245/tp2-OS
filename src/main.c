#include <signal.h>
#include "../lib/log.h"
#include "scoreboard.h"
#include "../test/basic-tests.h"
#include "../lib/library.h"

/**
 * @brief Entry point:
 *  - Loads scoreboard
 *  - Runs all tests (with sub-process + timeout logic)
 *  - Shows final scoreboard
 *  - Returns final numeric score
 */
int main(void){
     /* Set desired log level. */
     set_log_level(LOG_LEVEL_INFO);

     /* Install signal handlers to save scoreboard on INT/TERM. */
     signal(SIGINT, handle_signal);
     signal(SIGTERM, handle_signal);

     /* Load scoreboard (if scoreboard.json exists). */
     scoreboard_load();
     /* Example: we set HPC bonus on by default. */
     scoreboard_set_sc_hpc(1);

     /* Run all tests. Each test logs PASS/FAIL and updates scoreboard. */
     run_all_tests();

     /* Show scoreboard + save it. */
     show_scoreboard();
     scoreboard_save();

     /* Return final integer score as the program exit code. */
     int final_score = scoreboard_get_final_score();
     log_info("Final Score = %d", final_score);
     return final_score;
}
