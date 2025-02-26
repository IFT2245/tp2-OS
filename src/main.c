#include <stdio.h>
#include <stdlib.h>
#include "safe_calls_library.h"

/* ----------------------------------------------------------------
   MAIN
   ----------------------------------------------------------------
*/
int main(const int argc, char** argv) {
    (void)argc;
    (void)argv;

    scoreboard_init();
    scoreboard_load();
    os_init();
    stats_init();

    /* Register signals */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* Enter the main menu loop (never returns unless user chooses Exit) */
    menu_main_loop();

    /* If we ever break out, do final cleanup. */
    const int fs = scoreboard_get_final_score();
    cleanup_and_exit(fs);
    /* No code below here is reachable. */
    return 0;
}
