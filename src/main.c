#include "worker.h"
#include "../lib/log.h"
#include "../lib/scoreboard.h"
#include "../lib/library.h"
#include "../test/basic-tests.h"  // Now includes our expanded tests

static void show_scoreboard_submenu(void) {
    while(1) {
        printf("\n\033[1m\033[36m=== SCOREBOARD MENU ===\033[0m\n");
        printf("1) Show scoreboard legend\n");
        printf("2) Return to main menu\n");
        printf("Choice? ");
        fflush(stdout);
        char choice[64];
        if(!fgets(choice, sizeof(choice), stdin)) break;
        switch(choice[0]) {
        case '1':
            show_legend();
            break;
        case '2':
            return;
        default:
            printf("\033[33mUnknown option.\033[0m\n");
            break;
        }
    }
}


static void do_one_run_test(void){
    scoreboard_clear();
    scoreboard_save();
    set_log_level(LOG_LEVEL_INFO);
    run_all_tests();       // The refined test suite (includes multi-tests)
    show_scoreboard();
    scoreboard_save();
    fflush(stdout);
    fflush(stderr);
}

/**
 * We run all refined tests, show the scoreboard, and exit with final score.
 * OFFICIAL SCORING FUNCTION
 */
static void do_run_tests(void){
    scoreboard_clear();
    scoreboard_save();
    int i = 0;
    while (i++<10) {
        set_log_level(LOG_LEVEL_INFO);
        run_all_tests();       // The refined test suite (includes multi-tests)
        show_scoreboard();
        scoreboard_save();
        fflush(stdout);
        fflush(stderr);
    }
}


static void show_main_menu(void){
    fflush(stdout);
    fflush(stderr);
    printf("\n\033[1m\033[35m=== MAIN MENU ===\033[0m\n");
    printf("1) Run all tests\n");
    printf("2) Scoreboard\n");
    printf("3) Clear scoreboard\n");
    printf("4) Enable/Disable bonus test\n");
    printf(CLR_BOLD"5) NON OFFICIAL GRADING\n"CLR_RESET);
    printf("6) Exit\n");
    printf("Choice? ");
}

int main(void){
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    scoreboard_load();
    scoreboard_set_sc_hpc(1);
    set_bonus_test(1);   // default: bonus test ON

    while(1){
        if (fflush(stdout) == 0 && fflush(stderr) == 0) {
            usleep(50000);
            show_main_menu();
        }
        char choice[64];
        if(!fgets(choice, sizeof(choice), stdin)){
            break;
        }
        switch(choice[0]){
        case '1':
            init_preempt_timer();
            do_one_run_test();
            disable_preempt_timer();
            break;
        case '2':
            show_scoreboard();
            show_scoreboard_submenu();
            break;
        case '3':
            scoreboard_clear();
            printf("\033[31mScoreboard cleared.\033[00m\n");
            break;
        case '4':
            if (is_bonus_test()) {
                set_bonus_test(0);
                printf("\033[35mBonus test disabled.\033[0m\n");
            } else {
                set_bonus_test(1);
                printf("\033[35mBonus test enabled.\033[0m\n");
            }
            break;
        case '5':
            init_preempt_timer();
            do_run_tests();
            disable_preempt_timer();
            break;
        case '6':
        case 'q':
        case 'Q':
            printf("Exiting...\n");
            return scoreboard_get_final_score();
        default:
            printf("\033[33mUnknown option.\033[0m\n");
            break;
        }
    }
    return -1;
}
