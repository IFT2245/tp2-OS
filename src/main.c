#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "runner.h"
#include "os.h"
#include "safe_calls_library.h"
#include "scoreboard.h"
#include "stats.h"

/* Test suite headers. */
#include "../test/basic-test.h"
#include "../test/normal-test.h"
#include "../test/modes-test.h"
#include "../test/edge-test.h"
#include "../test/hidden-test.h"

/* ---------------------------------------------------------
   Implementation of functions declared in main.h
   --------------------------------------------------------- */

void clear_screen(void) {
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

void pause_enter(void) {
    printf("\nPress ENTER to continue...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* discard */
    }
}

int read_line(char *buf, size_t sz) {
    if (!fgets(buf, sz, stdin)) return 0;
    buf[strcspn(buf, "\n")] = '\0';
    return 1;
}

void ascii_main_menu_header(void) {
    printf(CLR_BOLD CLR_YELLOW "┌────────────────────────────────────────────┐\n" CLR_RESET);
    printf(CLR_BOLD CLR_YELLOW "│            OS-SCHEDULING GAME            │\n" CLR_RESET);
    printf(CLR_BOLD CLR_YELLOW "└────────────────────────────────────────────┘\n" CLR_RESET);
    printf("     'A concurrency and scheduling trainer'  \n\n");
}

void menu_show_scoreboard(void) {
    scoreboard_t sb;
    get_scoreboard(&sb);

    /* For test unlocking checks: */
    int unlockedB      = scoreboard_is_unlocked(SUITE_BASIC);
    int unlockedN      = scoreboard_is_unlocked(SUITE_NORMAL);
    int unlockedExt    = scoreboard_is_unlocked(SUITE_EXTERNAL);
    int unlockedModes  = scoreboard_is_unlocked(SUITE_MODES);
    int unlockedEdge   = scoreboard_is_unlocked(SUITE_EDGE);
    int unlockedHidden = scoreboard_is_unlocked(SUITE_HIDDEN);

    clear_screen();
    printf(CLR_BOLD CLR_MAGENTA "╔════════════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_MAGENTA "║           ★ SCOREBOARD OVERVIEW ★         ║\n" CLR_RESET);
    printf("║--------------------------------------------║\n");

    printf("║ BASIC       => %.1f/100 => %s\n",
           sb.basic_percent,
           unlockedB ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);

    printf("║ NORMAL      => %.1f/100 => %s\n",
           sb.normal_percent,
           unlockedN ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);

    printf("║ EXTERNAL    => %.1f/100 => %s\n",
           sb.external_percent,
           unlockedExt ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);

    printf("║ MODES       => %.1f/100 => %s\n",
           sb.modes_percent,
           unlockedModes ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);

    printf("║ EDGE        => %.1f/100 => %s\n",
           sb.edge_percent,
           unlockedEdge ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);

    printf("║ HIDDEN      => %.1f/100 => %s\n",
           sb.hidden_percent,
           unlockedHidden ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);

    printf("║--------------------------------------------║\n");
    printf("║ Schedulers mastery (10%% block => 15 points total):\n");
    printf("║   FIFO:%s  RR:%s  CFS:%s  CFS-SRTF:%s  BFS:%s\n",
           sb.sc_fifo? "✔":"✘",
           sb.sc_rr? "✔":"✘",
           sb.sc_cfs? "✔":"✘",
           sb.sc_cfs_srtf? "✔":"✘",
           sb.sc_bfs? "✔":"✘");

    printf("║   SJF:%s  STRF:%s  HRRN:%s  HRRN-RT:%s  PRIORITY:%s\n",
           sb.sc_sjf? "✔":"✘",
           sb.sc_strf? "✔":"✘",
           sb.sc_hrrn? "✔":"✘",
           sb.sc_hrrn_rt? "✔":"✘",
           sb.sc_priority? "✔":"✘");

    printf("║   HPC-OVER:%s  MLFQ:%s\n",
           sb.sc_hpc_over? "✔":"✘",
           sb.sc_mlfq? "✔":"✘");

    int final_score = scoreboard_get_final_score();
    printf("║--------------------------------------------║\n");
    printf("╚═ Overall Score => %d/100\n", final_score);

    pause_enter();
}

void menu_clear_scoreboard(void) {
    scoreboard_clear();
    printf("\nScoreboard cleared.\n");
    pause_enter();
}

void menu_toggle_speed_mode(void) {
    int current = stats_get_speed_mode();
    int next    = (current == 0) ? 1 : 0;
    stats_set_speed_mode(next);
    printf("\nSpeed mode set to: %s\n", (next == 0) ? "NORMAL" : "FAST");
    pause_enter();
}

void submenu_run_single_test(void) {
    clear_screen();
    printf("Choose which suite?\n");
    printf(" 1) Basic\n");
    printf(" 2) Normal\n");
    printf(" 3) Modes\n");
    printf(" 4) Edge\n");
    printf(" 5) Hidden\n");
    printf(" 6) External\n");
    printf("Choice: ");

    char buf[256];
    if(!read_line(buf, sizeof(buf))) return;
    int suite = parse_int_strtol(buf, -1);
    if(suite < 1 || suite > 6){
        printf("Invalid.\n");
        pause_enter();
        return;
    }

    /* Check if unlocked first: */
    int unlocked = 0;
    switch(suite) {
        case 1: unlocked = scoreboard_is_unlocked(SUITE_BASIC);    break;
        case 2: unlocked = scoreboard_is_unlocked(SUITE_NORMAL);   break;
        case 3: unlocked = scoreboard_is_unlocked(SUITE_MODES);    break;
        case 4: unlocked = scoreboard_is_unlocked(SUITE_EDGE);     break;
        case 5: unlocked = scoreboard_is_unlocked(SUITE_HIDDEN);   break;
        case 6: unlocked = scoreboard_is_unlocked(SUITE_EXTERNAL); break;
        default: break;
    }
    if(!unlocked) {
        printf("That suite is locked.\n");
        pause_enter();
        return;
    }

    /* We currently run the entire suite in place of "single test." */
    printf("\nRunning that suite's tests...\n");
    switch(suite){
        case 1: {
            int t=0, p=0;
            run_basic_tests(&t,&p);
            scoreboard_update_basic(t,p);
            break;
        }
        case 2: {
            int t=0, p=0;
            run_normal_tests(&t,&p);
            scoreboard_update_normal(t,p);
            break;
        }
        case 3: {
            int t=0, p=0;
            run_modes_tests(&t,&p);
            scoreboard_update_modes(t,p);
            break;
        }
        case 4: {
            int t=0, p=0;
            run_edge_tests(&t,&p);
            scoreboard_update_edge(t,p);
            break;
        }
        case 5: {
            int t=0, p=0;
            run_hidden_tests(&t,&p);
            scoreboard_update_hidden(t,p);
            break;
        }
        case 6: {
            run_external_tests_menu();
            break;
        }
        default:
            break;
    }
    scoreboard_save();
    pause_enter();
}

void submenu_run_tests(void) {
    /* This runs all unlocked test suites in order, with ASCII feedback. */
    printf(CLR_CYAN "╔═══════════════════════════════════════════╗\n");
    printf("║         Running all UNLOCKED tests        ║\n");
    printf("╚═══════════════════════════════════════════╝\n" CLR_RESET);

    /* BASIC */
    if(!scoreboard_is_unlocked(SUITE_BASIC)){
        printf("BASIC locked, skipping.\n");
    } else {
        int t=0,p=0;
        run_basic_tests(&t,&p);
        scoreboard_update_basic(t,p);
        scoreboard_save();
    }

    /* NORMAL */
    if(!scoreboard_is_unlocked(SUITE_NORMAL)){
        printf("NORMAL locked, skipping.\n");
    } else {
        int t=0,p=0;
        run_normal_tests(&t,&p);
        scoreboard_update_normal(t,p);
        scoreboard_save();
    }

    /* MODES */
    if(!scoreboard_is_unlocked(SUITE_MODES)){
        printf("MODES locked, skipping.\n");
    } else {
        int t=0,p=0;
        run_modes_tests(&t,&p);
        scoreboard_update_modes(t,p);
        scoreboard_save();
    }

    /* EDGE */
    if(!scoreboard_is_unlocked(SUITE_EDGE)){
        printf("EDGE locked, skipping.\n");
    } else {
        int t=0,p=0;
        run_edge_tests(&t,&p);
        scoreboard_update_edge(t,p);
        scoreboard_save();
    }

    /* HIDDEN */
    if(!scoreboard_is_unlocked(SUITE_HIDDEN)){
        printf("HIDDEN locked, skipping.\n");
    } else {
        int t=0,p=0;
        run_hidden_tests(&t,&p);
        scoreboard_update_hidden(t,p);
        scoreboard_save();
    }

    /* EXTERNAL (in some designs you might want to run them as well) */
    if(!scoreboard_is_unlocked(SUITE_EXTERNAL)){
        printf("EXTERNAL locked, skipping.\n");
    } else {
        printf("Running external tests...\n");
        run_external_tests_menu();
        scoreboard_save();
    }

    pause_enter();
}

void menu_submenu_external_concurrency(void) {
    int unlockedExt = scoreboard_is_unlocked(SUITE_EXTERNAL);
    if(!unlockedExt) {
        printf("\n[External Concurrency] is locked.\n");
        pause_enter();
        return;
    }

    printf(CLR_BOLD CLR_CYAN "\n╔══════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN   "║ External Shell Concurrency   ║\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN   "╚══════════════════════════════╝\n" CLR_RESET);

    printf("1) Run concurrency with a SINGLE scheduling mode\n");
    printf("2) Run concurrency with ALL scheduling modes\n");
    printf("\nChoice: ");
    char buf[256];
    if(!read_line(buf, sizeof(buf))) return;
    int sub = parse_int_strtol(buf, -1);
    if(sub < 1 || sub > 2){
        printf("Invalid.\n");
        pause_enter();
        return;
    }

    printf("How many concurrent shells? ");
    if(!read_line(buf, sizeof(buf))) return;
    int n = parse_int_strtol(buf, 0);
    if(n<1){
        printf("Invalid number of shells.\n");
        pause_enter();
        return;
    }

    printf("How many CPU cores? ");
    if(!read_line(buf, sizeof(buf))) return;
    int c = parse_int_strtol(buf, 2);
    if(c<1) c=2;

    /* concurrency level => short/medium/stress */
    printf("\nChoose concurrency test type:\n");
    printf(" 1) Short test\n");
    printf(" 2) Medium test\n");
    printf(" 3) Stress test\n");
    printf("Choice: ");
    if(!read_line(buf,sizeof(buf))) return;
    int style = parse_int_strtol(buf,1);
    if(style<1 || style>3) style=1;

    char** lines = (char**)calloc(n, sizeof(char*));
    if(!lines) return;

    /* Prepare lines for concurrency. */
    stats_inc_concurrency_runs();

    if(sub==1){
        /* single scheduling => vary 'sleep' based on style. */
        for(int i=0; i<n; i++){
            int base=2;
            switch(style){
                case 1: base=2; break;
                case 2: base=5; break;
                case 3: base=10;break;
            }
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "sleep %d", (i+1)*base);
            lines[i] = strdup(tmp);
        }
    } else {
        /* sub==2 => run all scheduling modes => we keep it simple. */
        for(int i=0; i<n; i++){
            snprintf(buf,sizeof(buf),"sleep %d", (i+1)*2);
            lines[i] = strdup(buf);
        }
    }

    if(sub==1){
        printf("\nSelect scheduling mode:\n");
        printf(" 0=FIFO,1=RR,2=CFS,3=CFS-SRTF,4=BFS,\n");
        printf(" 5=SJF,6=STRF,7=HRRN,8=HRRN-RT,\n");
        printf(" 9=PRIORITY,10=HPC-OVER,11=MLFQ\n");
        printf("Choice: ");
        if(!read_line(buf,sizeof(buf))){
            pause_enter();
            for(int i=0;i<n;i++) free(lines[i]);
            free(lines);
            return;
        }
        int mode = parse_int_strtol(buf, -1);
        if(mode<0 || mode>11){
            printf("Invalid mode.\n");
            pause_enter();
        } else {
            run_shell_commands_concurrently(n, lines, c, mode, 0);
        }
    } else {
        run_shell_commands_concurrently(n, lines, c, -1, 1);
    }

    for(int i=0; i<n; i++){
        free(lines[i]);
    }
    free(lines);
    pause_enter();
}

void cleanup_and_exit(int code) {
    os_cleanup();
    scoreboard_save();
    scoreboard_close();
    stats_print_summary();
    exit(code);
}

/* Handle signals => SIGINT, SIGTERM, SIGUSR1 => store concurrency_stop if needed. */
void handle_signal(int signum) {
    if(signum == SIGINT) {
        stats_inc_signal_sigint();
        printf("\n[Main] Caught SIGINT! Saving scoreboard...\n");
        int fs = scoreboard_get_final_score();
        cleanup_and_exit(fs);
    }
    else if(signum == SIGTERM){
        stats_inc_signal_sigterm();
        printf("\n[Main] Caught SIGTERM! Saving scoreboard...\n");
        int fs = scoreboard_get_final_score();
        cleanup_and_exit(fs);
    }
    else if(signum == SIGUSR1) {
        stats_inc_signal_other();
        printf("\n[Main] Caught SIGUSR1 => concurrency stop flag set.\n");
        set_os_concurrency_stop_flag(1); /* unify concurrency stop in os.c */
    }
}

/* ---------------------------------------------------------
   The main() entry point
   --------------------------------------------------------- */
int main(int argc, char** argv){
    (void)argc;
    (void)argv;

    /* Hook signals. */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);

    scoreboard_init();
    scoreboard_load();
    os_init();
    stats_init();

    while(1){
        clear_screen();
        ascii_main_menu_header();

        printf(CLR_BOLD CLR_YELLOW "┌─── MAIN MENU ─────────────────────────────┐\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 1) Run All Unlocked Test Suites           │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 2) Exit                                   │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 3) External Shell Concurrency            │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 4) External Tests                         │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 5) Show Scoreboard                        │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 6) Clear Scoreboard                       │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 7) Toggle Speed Mode                      │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 8) Run Single Test (Submenu)             │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "└────────────────────────────────────────────┘\n\n" CLR_RESET);
        printf("Choice: ");

        char input[256];
        if(!read_line(input, sizeof(input))){
            printf("Exiting (EOF/read error).\n");
            int fs = scoreboard_get_final_score();
            cleanup_and_exit(fs);
        }
        int choice = parse_int_strtol(input, -1);

        switch(choice){
        case 1:
            submenu_run_tests();
            break;

        case 2: {
            int fs = scoreboard_get_final_score();
            printf("\nExiting with final score = %d.\n", fs);
            cleanup_and_exit(fs);
            break;
        }

        case 3:
            menu_submenu_external_concurrency();
            break;

        case 4: {
            if(!scoreboard_is_unlocked(SUITE_EXTERNAL)){
                printf("External tests locked.\n");
                pause_enter();
            } else {
                printf("\nRunning external tests...\n");
                run_external_tests_menu();
                scoreboard_save();
                pause_enter();
            }
            break;
        }

        case 5:
            menu_show_scoreboard();
            break;

        case 6:
            menu_clear_scoreboard();
            break;

        case 7:
            menu_toggle_speed_mode();
            break;

        case 8:
            submenu_run_single_test();
            break;

        default:
            printf("Invalid.\n");
            pause_enter();
            break;
        }
    }

    return 0;
}
