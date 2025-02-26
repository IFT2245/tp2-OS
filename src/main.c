
//   MAIN.C
#include "menu.h"

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

/* ----------------------------------------------------------------
   SHARED UTILS
   ----------------------------------------------------------------
*/
void pause_enter(void) {
    printf(CLR_CYAN CLR_BOLD "\nPress ENTER to continue..." CLR_RESET);
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* discard leftover */
    }
}

ssize_t read_line(char *buf, const size_t sz) {
    if (os_concurrency_stop_requested()) {
        /* If a stop is requested, return 0 so that callers can decide to exit or skip. */
        return 0;
    }

    if (buf == NULL || sz == 0 || sz > INT_MAX) {
        return 0;
    }

    if (fgets(buf, (int)sz, stdin) == NULL) {
        if (ferror(stdin)) {
            clearerr(stdin);
        }
        return 0;
    }

    size_t newline_pos = strcspn(buf, "\n");
    buf[newline_pos] = '\0';
    return 1;
}
