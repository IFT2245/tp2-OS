#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "os.h"
#include "scoreboard.h"
#include "stats.h"
#include "menu.h"

/* ----------------------------------------------------------------
   SIGNAL HANDLER
   ----------------------------------------------------------------
*/
static void handle_signal(int signum) {
    if(signum == SIGINT) {
        /* SIGINT => exit immediately, but save scoreboard + stats. */
        stats_inc_signal_sigint();
        printf("\nCaught SIGINT => Exiting.\n");
        int fs = scoreboard_get_final_score();
        os_cleanup();
        scoreboard_save();
        scoreboard_close();
        stats_print_summary();
        exit(fs);
    }
    if(signum == SIGTERM) {
        /* SIGTERM => stop concurrency/tests => return to menu. */
        stats_inc_signal_sigterm();
        printf(CLR_RED "\nCaught SIGTERM => Trying to return to menu\n");
        set_os_concurrency_stop_flag(1);
        menu_main_loop();
    }
}

/* ----------------------------------------------------------------
   MAIN
   ----------------------------------------------------------------
*/
int main(int argc, char** argv) {
    (void)argc; (void)argv;

    scoreboard_init();
    scoreboard_load();
    os_init();
    stats_init();

    /* Register signals */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);

    /* Enter the main menu loop (never returns unless user chooses Exit) */
    menu_main_loop();

    /* If we ever break out, do final cleanup. */
    int fs = scoreboard_get_final_score();
    os_cleanup();
    scoreboard_save();
    scoreboard_close();
    stats_print_summary();
    return fs;
}
