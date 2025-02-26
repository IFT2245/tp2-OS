#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "runner.h"
#include "os.h"
#include "safe_calls_library.h"
#include "scoreboard.h"
#include "stats.h"

/* Each suite's public test functions */
#include <stdio.h>

#include "../test/basic-test.h"
#include "../test/normal-test.h"
#include "../test/modes-test.h"
#include "../test/edge-test.h"
#include "../test/hidden-test.h"
#include "../test/external-test.h"

/*
  Flag set by SIGTERM to stop concurrency or test suites
  but remain in the main menu.
*/
static volatile sig_atomic_t g_return_to_menu = 0;

/* The chain of suites we might unlock in order. */
static scoreboard_suite_t g_suite_chain[] = {
    SUITE_BASIC,
    SUITE_NORMAL,
    SUITE_EXTERNAL,
    SUITE_MODES,
    SUITE_EDGE,
    SUITE_HIDDEN
};
static const int g_suite_chain_count = 6;

/* ----------------------------------------------------------------
   PLATFORM-SPECIFIC CLEAR SCREEN
   ----------------------------------------------------------------
*/
void clear_screen(void) {
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

/* ----------------------------------------------------------------
   Wait for ENTER so user can read output
   ----------------------------------------------------------------
*/
void pause_enter(void) {
    printf( CLR_CYAN CLR_BOLD"\nPress ENTER to continue..." CLR_RESET);
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* discard leftover */
    }
}

/* ----------------------------------------------------------------
   Read line safely from stdin
   ----------------------------------------------------------------
*/
int read_line(char *buf, size_t sz) {
    // If we've requested a return to menu, skip reading altogether
    if (g_return_to_menu) {
        return 0;  // signals "no input" so caller can exit
    }

    if (!fgets(buf, sz, stdin)) {
        return 0;  // error or EOF
    }

    buf[strcspn(buf, "\n")] = '\0'; // remove newline
    return 1;
}


/* ----------------------------------------------------------------
   Print the fancy main menu header + speed mode status
   ----------------------------------------------------------------
*/
void ascii_main_menu_header(void) {
    clear_screen();

    int sp = stats_get_speed_mode(); /* 0 => NORMAL, 1 => FAST */
    const char* sp_text = (sp == 0) ? "NORMAL" : "FAST";

    printf(CLR_BOLD CLR_YELLOW " ┌────────────────────────────────────────────┐\n" CLR_RESET);
    printf(CLR_BOLD CLR_YELLOW " │             OS-SCHEDULING GAME             │\n" CLR_RESET);
    printf(CLR_BOLD CLR_YELLOW " └────────────────────────────────────────────┘\n" CLR_RESET);
    printf(CLR_RED "     A concurrency and scheduling trainer   \n" CLR_RESET);
    printf(CLR_BOLD CLR_RED"          [Current Speed Mode: %s]\n\n" CLR_RESET, sp_text);
}

/* ----------------------------------------------------------------
   Display scoreboard with fancy ASCII framing + progress bar
   ----------------------------------------------------------------
*/
void menu_show_scoreboard(void) {
    scoreboard_t sb;
    get_scoreboard(&sb);

    int unlockedB      = scoreboard_is_unlocked(SUITE_BASIC);
    int unlockedN      = scoreboard_is_unlocked(SUITE_NORMAL);
    int unlockedExt    = scoreboard_is_unlocked(SUITE_EXTERNAL);
    int unlockedModes  = scoreboard_is_unlocked(SUITE_MODES);
    int unlockedEdge   = scoreboard_is_unlocked(SUITE_EDGE);
    int unlockedHidden = scoreboard_is_unlocked(SUITE_HIDDEN);

    clear_screen();
    printf(CLR_BOLD CLR_MAGENTA "╔════════════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_MAGENTA "║           ★ SCOREBOARD OVERVIEW ★          ║\n" CLR_RESET);
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
    printf("║  FIFO:%s RR:%s CFS:%s CFS-SRTF:%s BFS:%s\n",
           sb.sc_fifo? "✔":"✘",
           sb.sc_rr? "✔":"✘",
           sb.sc_cfs? "✔":"✘",
           sb.sc_cfs_srtf? "✔":"✘",
           sb.sc_bfs? "✔":"✘");
    printf("║  SJF:%s STRF:%s HRRN:%s HRRN-RT:%s PRIORITY:%s\n",
           sb.sc_sjf? "✔":"✘",
           sb.sc_strf? "✔":"✘",
           sb.sc_hrrn? "✔":"✘",
           sb.sc_hrrn_rt? "✔":"✘",
           sb.sc_priority? "✔":"✘");
    printf("║  HPC-OVER:%s MLFQ:%s\n",
           sb.sc_hpc_over? "✔":"✘",
           sb.sc_mlfq? "✔":"✘");

    const int final_score = scoreboard_get_final_score();
    printf("║--------------------------------------------║\n");
    printf("╠═"CLR_BOLD CLR_CYAN" Overall Score => " CLR_RESET CLR_BOLD CLR_RED "%d/100" CLR_RESET "\n", final_score);

    /* Simple progress bar for final score */
    printf("╠═" CLR_BOLD CLR_CYAN" Progress: " CLR_RESET);
    int barLength = final_score / 10; /* each block = 10 points */
    printf("[");
    for(int i=0; i<barLength; i++){
        printf("█");
    }
    for(int i=barLength; i<10; i++){
        printf(" ");
    }
    printf("]" CLR_RED "%d%%\n" CLR_RESET, final_score);

    printf("╚════════════════════════════════════════════╝\n");

    pause_enter();
}

/* ----------------------------------------------------------------
   Clears scoreboard entirely
   ----------------------------------------------------------------
*/
void menu_clear_scoreboard(void) {
    scoreboard_clear();
    printf(CLR_BOLD CLR_CYAN "\n╔════════════════════════════════════════════╗\n");
    printf("║ Scoreboard cleared.                        ║\n");
    printf("╚════════════════════════════════════════════╝\n" CLR_RESET);
    pause_enter();
}

/* ----------------------------------------------------------------
   Toggles 0=>normal, 1=>fast
   ----------------------------------------------------------------
*/
void menu_toggle_speed_mode(void) {
    int current = stats_get_speed_mode();
    int next    = (current == 0) ? 1 : 0;
    stats_set_speed_mode(next);

    printf(CLR_BOLD CLR_CYAN "\n╔═════════════════════════════════════╗\n");
    printf("║ Speed mode set to: %s\n", (next == 0) ? "NORMAL" : "FAST");
    printf("╚═════════════════════════════════════╝\n" CLR_RESET);

    pause_enter();
}

/* ----------------------------------------------------------------
   Attempt to prompt user to run the next suite if newly unlocked
   (the chain: Basic->Normal->External->Modes->Edge->Hidden).
   Called after finishing a single suite or single test in that suite.
   ----------------------------------------------------------------
*/
static void attempt_run_next_suite(scoreboard_suite_t currentSuite) {
    int found_idx = -1;
    for(int i=0; i<g_suite_chain_count; i++){
        if(g_suite_chain[i] == currentSuite){
            found_idx = i;
            break;
        }
    }
    if(found_idx < 0) return; /* not found in chain */
    if(found_idx == g_suite_chain_count-1) return; /* last suite => no next */

    scoreboard_suite_t next = g_suite_chain[found_idx+1];
    if(scoreboard_is_unlocked(next)) {
        printf(CLR_BOLD CLR_GREEN "\n┌───────────────────────────────────────────────────────────┐\n");
        printf("│"CLR_GREEN" Good job! Next suite is now unlocked"CLR_RESET"                │\n");
        printf("│"CLR_GREEN CLR_BOLD" Do you want to run the next suite immediately? (y/n)"CLR_RESET"      │\n");
        printf("└───────────────────────────────────────────────────────────┘\n" CLR_RESET);
        printf("Choice: ");

        char buf[256];
        if(!read_line(buf, sizeof(buf))) {
            return;
        }
        if(buf[0] == 'y' || buf[0] == 'Y') {
            /* directly run that suite (all its tests) as if single suite run */
            switch(next) {
                case SUITE_NORMAL: {
                    int t=0, p=0;
                    run_normal_tests(&t,&p);
                    scoreboard_update_normal(t,p);
                    scoreboard_save();
                    break;
                }
                case SUITE_EXTERNAL: {
                    run_external_tests_menu();
                    scoreboard_save();
                    break;
                }
                case SUITE_MODES: {
                    int t=0, p=0;
                    run_modes_tests(&t,&p);
                    scoreboard_update_modes(t,p);
                    scoreboard_save();
                    break;
                }
                case SUITE_EDGE: {
                    int t=0, p=0;
                    run_edge_tests(&t,&p);
                    scoreboard_update_edge(t,p);
                    scoreboard_save();
                    break;
                }
                case SUITE_HIDDEN: {
                    int t=0, p=0;
                    run_hidden_tests(&t,&p);
                    scoreboard_update_hidden(t,p);
                    scoreboard_save();
                    break;
                }
                default: /* no-op */ break;
            }
            /* chain further if that unlocked more */
            attempt_run_next_suite(next);
        }
    }
}

/* ----------------------------------------------------------------
   Runs exactly one test from the chosen suite, i.e. not the entire suite.
   We ask the suite for how many tests it has, then which one to run.
   We update scoreboard by +1 test total, and +1 pass if it passed.
   Then save, and see if next suite unlocks.
   ----------------------------------------------------------------
*/
static void run_single_test_in_suite(scoreboard_suite_t chosen) {
    /* we must see how many test-cases are in that suite, let user pick. */
    int count = 0;
    switch(chosen) {
        case SUITE_BASIC:   count = basic_test_count();   break;
        case SUITE_NORMAL:  count = normal_test_count();  break;
        case SUITE_MODES:   count = modes_test_count();   break;
        case SUITE_EDGE:    count = edge_test_count();    break;
        case SUITE_HIDDEN:  count = hidden_test_count();  break;
        case SUITE_EXTERNAL:count = external_test_count();break;
        default: count=0; break;
    }
    if(count<=0) {
        printf("No tests found in that suite or suite missing.\n");
        pause_enter();
        return;
    }

    printf("\nWhich single test do you want to run (1..%d)? ", count);
    char buf[256];
    if(!read_line(buf, sizeof(buf))) return;
    int pick = parse_int_strtol(buf, -1);
    if(pick<1 || pick>count) {
        printf("Invalid test index.\n");
        pause_enter();
        return;
    }

    /* run that single test now. We'll get pass/fail result. */
    printf("\nRunning test #%d in that suite...\n\n", pick);

    int passResult = 0; /* 0 => fail, 1 => pass */
    switch(chosen){
        case SUITE_BASIC:
            basic_test_run_single(pick-1, &passResult); /* 0-based index */
            scoreboard_update_basic(1, passResult);
            scoreboard_save();
            attempt_run_next_suite(SUITE_BASIC);
            break;
        case SUITE_NORMAL:
            normal_test_run_single(pick-1, &passResult);
            scoreboard_update_normal(1, passResult);
            scoreboard_save();
            attempt_run_next_suite(SUITE_NORMAL);
            break;
        case SUITE_MODES:
            modes_test_run_single(pick-1, &passResult);
            scoreboard_update_modes(1, passResult);
            scoreboard_save();
            attempt_run_next_suite(SUITE_MODES);
            break;
        case SUITE_EDGE:
            edge_test_run_single(pick-1, &passResult);
            scoreboard_update_edge(1, passResult);
            scoreboard_save();
            attempt_run_next_suite(SUITE_EDGE);
            break;
        case SUITE_HIDDEN:
            hidden_test_run_single(pick-1, &passResult);
            scoreboard_update_hidden(1, passResult);
            scoreboard_save();
            attempt_run_next_suite(SUITE_HIDDEN);
            break;
        case SUITE_EXTERNAL:
            external_test_run_single(pick-1, &passResult);
            scoreboard_update_external(1, passResult);
            scoreboard_save();
            attempt_run_next_suite(SUITE_EXTERNAL);
            break;
        default:
            break;
    }

    pause_enter();
}

/* ----------------------------------------------------------------
   Menu option "Run Single Test (Submenu)" => asks user which suite,
   then calls run_single_test_in_suite(). If suite locked => fail.
   ----------------------------------------------------------------
*/
void submenu_run_single_test(void) {
    clear_screen();
    printf(CLR_BOLD CLR_CYAN "╔════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN "║ RUN SINGLE TEST - SUITE SELECTION  ║\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN "╚════════════════════════════════════╝\n" CLR_RESET);

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

    scoreboard_suite_t chosen;
    switch(suite){
        case 1: chosen = SUITE_BASIC;   break;
        case 2: chosen = SUITE_NORMAL;  break;
        case 3: chosen = SUITE_MODES;   break;
        case 4: chosen = SUITE_EDGE;    break;
        case 5: chosen = SUITE_HIDDEN;  break;
        case 6: chosen = SUITE_EXTERNAL;break;
        default: return;
    }

    int unlocked = scoreboard_is_unlocked(chosen);
    if(!unlocked && chosen!=SUITE_BASIC) {
        /*
          Basic is always unlocked by design.
          Others must meet scoreboard pass threshold from the previous suite.
        */
        printf("\nThat suite is currently locked (need 60%% pass on the previous suite).\n");
        pause_enter();
        return;
    }

    run_single_test_in_suite(chosen);
}

/* ----------------------------------------------------------------
   "Run All Unlocked Test Suites" but skip re-running those at 100%.
   We run them in chain order, each suite's entire test set.
   If user sends SIGTERM => we break early.
   ----------------------------------------------------------------
*/
void submenu_run_tests(void) {
    g_return_to_menu = 0;
    scoreboard_t sb;
    get_scoreboard(&sb);

    printf(CLR_BOLD CLR_CYAN "╔═══════════════════════════════════════════╗\n");
    printf("║     Running all UNLOCKED (unpassed) tests ║\n");
    printf("╚═══════════════════════════════════════════╝\n" CLR_RESET);

    scoreboard_load(); /* refresh scoreboard just in case */

    for(int i=0; i<g_suite_chain_count; i++){
        scoreboard_suite_t st = g_suite_chain[i];
        if(!scoreboard_is_unlocked(st)) {
            /* skip locked */
            continue;
        }
        /* skip if that suite is fully at 100% pass */
        switch(st){
            case SUITE_BASIC:
                if(sb.basic_percent >= 100.0)  continue;
                break;
            case SUITE_NORMAL:
                if(sb.normal_percent >= 100.0) continue;
                break;
            case SUITE_EXTERNAL:
                if(sb.external_percent>=100.0) continue;
                break;
            case SUITE_MODES:
                if(sb.modes_percent>=100.0)    continue;
                break;
            case SUITE_EDGE:
                if(sb.edge_percent>=100.0)     continue;
                break;
            case SUITE_HIDDEN:
                if(sb.hidden_percent>=100.0)   continue;
                break;
            default:
                break;
        }

        /* if user used SIGTERM => break out */
        if(g_return_to_menu) {
            printf("[RunAllUnlocked] SIGTERM => returning.\n");
            break;
        }

        /* run that entire suite's test set now. */
        switch(st){
            case SUITE_BASIC: {
                int t=0,p=0;
                printf("\n[Running BASIC suite...]\n");
                run_basic_tests(&t,&p);
                scoreboard_update_basic(t,p);
                scoreboard_save();
                attempt_run_next_suite(SUITE_BASIC);
                break;
            }
            case SUITE_NORMAL: {
                int t=0,p=0;
                printf("\n[Running NORMAL suite...]\n");
                run_normal_tests(&t,&p);
                scoreboard_update_normal(t,p);
                scoreboard_save();
                attempt_run_next_suite(SUITE_NORMAL);
                break;
            }
            case SUITE_EXTERNAL: {
                printf("\n[Running EXTERNAL suite...]\n");
                run_external_tests_menu();
                scoreboard_save();
                attempt_run_next_suite(SUITE_EXTERNAL);
                break;
            }
            case SUITE_MODES: {
                int t=0,p=0;
                printf("\n[Running MODES suite...]\n");
                run_modes_tests(&t,&p);
                scoreboard_update_modes(t,p);
                scoreboard_save();
                attempt_run_next_suite(SUITE_MODES);
                break;
            }
            case SUITE_EDGE: {
                int t=0,p=0;
                printf("\n[Running EDGE suite...]\n");
                run_edge_tests(&t,&p);
                scoreboard_update_edge(t,p);
                scoreboard_save();
                attempt_run_next_suite(SUITE_EDGE);
                break;
            }
            case SUITE_HIDDEN: {
                int t=0,p=0;
                printf("\n[Running HIDDEN suite...]\n");
                run_hidden_tests(&t,&p);
                scoreboard_update_hidden(t,p);
                scoreboard_save();
                attempt_run_next_suite(SUITE_HIDDEN);
                break;
            }
            default:
                break;
        }
        get_scoreboard(&sb); /* re-check scoreboard for next iteration */
    }

    pause_enter();
}

/* ----------------------------------------------------------------
   External concurrency menu
   ----------------------------------------------------------------
*/
void menu_submenu_external_concurrency(void) {
    int unlockedExt = scoreboard_is_unlocked(SUITE_EXTERNAL);
    if(!unlockedExt) {
        printf(CLR_BOLD CLR_RED "\n[External Concurrency] is locked (need 60%% pass on EXTERNAL suite).\n" CLR_RESET);
        pause_enter();
        return;
    }

    clear_screen();
    printf(CLR_BOLD CLR_CYAN "\n╔════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN   "║  External Shell Concurrency Menu   ║\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN   "╚════════════════════════════════════╝\n" CLR_RESET);

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

    printf("\nChoose concurrency test style:\n");
    printf(" 1) Short test\n");
    printf(" 2) Medium test\n");
    printf(" 3) Stress test\n");
    printf("Choice: ");
    if(!read_line(buf,sizeof(buf))) return;
    int style = parse_int_strtol(buf,1);
    if(style<1 || style>3) style=1;

    char** lines = (char**)calloc(n, sizeof(char*));
    if(!lines) return;

    g_return_to_menu = 0;

    int base=2;
    switch(style) {
        case 1: base=2;  break;  /* short */
        case 2: base=5;  break;  /* medium */
        case 3: base=10; break;  /* stress */
        default: base=2; break;
    }

    /* Build commands for each shell: "sleep X" */
    for(int i=0; i<n; i++){
        char tmp[64];
        if(sub == 1) {
            /* single mode => small differences in durations */
            snprintf(tmp, sizeof(tmp), "sleep %d", (i+1)*base);
        } else {
            /* all modes => short uniform sleeps */
            snprintf(tmp, sizeof(tmp), "sleep %d", (i+1)*2);
        }
        lines[i] = strdup(tmp);
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

/* ----------------------------------------------------------------
   Final cleanup => exit code.
   Print a final stats summary.
   ----------------------------------------------------------------
*/
void cleanup_and_exit(int code) {
    printf(CLR_BOLD CLR_YELLOW "\n╔════════════════════════════════════════════╗\n");
    printf("║        Exiting => Saving scoreboard        ║\n");
    printf("╚════════════════════════════════════════════╝\n" CLR_RESET);

    os_cleanup();
    scoreboard_save();
    scoreboard_close();
    stats_print_summary();
    exit(code);
}

/* ----------------------------------------------------------------
   Our signal handler
   ----------------------------------------------------------------
*/
void handle_signal(int signum) {
    if(signum == SIGINT) {
        stats_inc_signal_sigint();
        printf(CLR_BOLD CLR_RED "\n[Main] Caught SIGINT => Save scoreboard and exit.\n" CLR_RESET);
        int fs = scoreboard_get_final_score();
        cleanup_and_exit(fs);
    }
    else if(signum == SIGTERM){
        stats_inc_signal_sigterm();
        printf(CLR_BOLD CLR_RED "\n[Main] Caught SIGTERM => concurrency/test stops => returning to menu.\n" CLR_RESET);
        set_os_concurrency_stop_flag(1);
        g_return_to_menu = 1;
    }
    else if(signum == SIGUSR1) {
        stats_inc_signal_other();
        printf(CLR_BOLD CLR_RED "\n[Main] Caught SIGUSR1 => concurrency stop.\n" CLR_RESET);
        set_os_concurrency_stop_flag(1);
    }
}

/* ----------------------------------------------------------------
   main() => scoreboard init, os init, stats init
   Show main menu until user chooses exit
   ----------------------------------------------------------------
*/
int main(int argc, char** argv){
    (void)argc;
    (void)argv;

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);

    scoreboard_init();
    scoreboard_load();
    os_init();
    stats_init();

    while(1){
        ascii_main_menu_header();
        printf(CLR_BOLD CLR_YELLOW " ┌─── MAIN MENU ─────────────────────────────┐\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 1) Run All Failed Internal Suites         │\n" CLR_RESET);
        printf(CLR_BOLD CLR_GRAY   " │ 2) Exit                                   │\n" CLR_RESET);
        printf(CLR_BOLD CLR_GRAY   " │ 3) External Shell Concurrency DEMO        │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 4) Run External Tests                     │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 5) Show Scoreboard                        │\n" CLR_RESET);
        printf(CLR_BOLD CLR_GRAY   " │ 6) Clear Scoreboard                       │\n" CLR_RESET);
        printf(CLR_BOLD CLR_GRAY   " │ 7) Toggle Speed Mode                      │\n" CLR_RESET);
        printf(CLR_BOLD CLR_GRAY   " │ 8) Run Single Test (Submenu)              │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " └───────────────────────────────────────────┘\n\n" CLR_RESET);
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
                printf("External tests locked (need 60%% pass on EXTERNAL suite).\n");
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
