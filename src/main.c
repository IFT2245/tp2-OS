#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "../lib/scoreboard.h"
#include "../test/basic-tests.h"
#include "../lib/library.h"
#include "../lib/log.h"

extern void set_slow_mode(int onOff);
extern int  is_slow_mode(void);
extern void set_bonus_test(int onOff);
extern int  is_bonus_test(void);

static void do_run_tests(void){
    set_log_level(LOG_LEVEL_INFO);
    run_all_tests();    /* This calls your test your work */
    show_scoreboard();  /* Display after running. */
    scoreboard_save();
    fflush(stdout);
    fflush(stderr);
}

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

static void show_main_menu(void){
    fflush(stdout);
    fflush(stderr);
    if(is_slow_mode())
    {
        printf("\033[36mSlow motion activated\033[0m");
    } else
    {
        printf("\033[36mFast motion activated\033[0m");
    }
    printf("\n\033[1m\033[35m=== MAIN MENU ===\033[0m\n");
    printf("1) Run all tests\n");
    printf("2) Scoreboard\n");
    printf("3) Clear scoreboard\n");
    printf("4) Enable slow concurrency\n");
    printf("5) Disable slow concurrency\n");
    printf("6) Enable/Disable bonus test\n");
    printf("7) Exit\n");
    printf("Choice? ");
}

int main(void){
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    // HPC Bonus is initially ON for demonstration
    set_log_level(LOG_LEVEL_INFO);
    scoreboard_load();
    scoreboard_set_sc_hpc(1);
    set_slow_mode(0);    // default concurrency mode: fast
    set_bonus_test(0);   // default: bonus test OFF

    while(1){
        if (fflush(stdout) == 0 && fflush(stderr) == 0) {
            show_main_menu();
        }
        char choice[64];
        if(!fgets(choice, sizeof(choice), stdin)){
            break;
        }
        switch(choice[0]){
        case '1':
            do_run_tests();
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
            set_slow_mode(1);
            printf("\033[36mSlow concurrency enabled.\033[0m\n");
            break;
        case '5':
            set_slow_mode(0);
            printf("\033[36mSlow concurrency disabled.\033[00m\n");
            break;
        case '6':
            if (is_bonus_test()) {
                set_bonus_test(0);
                printf("\033[35mBonus test disabled.\033[0m\n");
            } else {
                set_bonus_test(1);
                printf("\033[35mBonus test enabled.\033[0m\n");
            }
            break;
        case '7':
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
