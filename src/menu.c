#include "menu.h"

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

/* Display scoreboard in a framed format. */
void menu_show_scoreboard(void) {
    scoreboard_t sb;
    get_scoreboard(&sb);
    printf("%d", stats_get_test_fifo()); // TO IMPLEMENT WITH STYLE
    printf(CLR_BOLD CLR_MAGENTA "╔════════════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_MAGENTA "║           ★ SCOREBOARD OVERVIEW ★          ║\n" CLR_RESET);
    printf("║--------------------------------------------║\n");

    /* Show each suite's pass% plus locked/unlocked. */
    int unlockedB      = scoreboard_is_unlocked(SUITE_BASIC);
    int unlockedN      = scoreboard_is_unlocked(SUITE_NORMAL);
    int unlockedExt    = scoreboard_is_unlocked(SUITE_EXTERNAL);
    int unlockedModes  = scoreboard_is_unlocked(SUITE_MODES);
    int unlockedEdge   = scoreboard_is_unlocked(SUITE_EDGE);
    int unlockedHidden = scoreboard_is_unlocked(SUITE_HIDDEN);

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
    printf("║--------------------------------------------║\n");

    /* final overall score with a progress bar. */
    const int final_score = scoreboard_get_final_score();
    printf("╠═" CLR_BOLD CLR_CYAN " Overall Score => " CLR_RESET CLR_BOLD CLR_RED "%d/100\n" CLR_RESET, final_score);
    printf("╠═" CLR_BOLD CLR_CYAN " Progress: " CLR_RESET);
    int barLength = final_score / 10;
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

void menu_clear_scoreboard(void) {
    scoreboard_clear();
    printf(CLR_BOLD CLR_CYAN "\n╔════════════════════════════════════════════╗\n");
    printf("║ Scoreboard cleared.                        ║\n");
    printf("╚════════════════════════════════════════════╝\n" CLR_RESET);
    pause_enter();
}

void menu_toggle_speed_mode(void) {
    int current = stats_get_speed_mode();
    int next    = (current == 0) ? 1 : 0;
    stats_set_speed_mode(next);

    printf(CLR_BOLD CLR_CYAN "\n╔═════════════════════════════════════╗\n");
    printf("║ Speed mode set to: %s\n", (next == 0) ? "NORMAL" : "FAST");
    printf("╚═════════════════════════════════════╝\n" CLR_RESET);

    pause_enter();
}

/*
   Submenu that asks user to pick a single test from a chosen suite
   => runs exactly that test.
*/
void submenu_run_single_test(void) {
    printf(CLR_BOLD CLR_CYAN "╔════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN "║ RUN SINGLE TEST - SUITE SELECTION  ║\n" CLR_RESET);
    printf(CLR_BOLD CLR_CYAN "╚════════════════════════════════════╝\n" CLR_RESET);

    /* We'll hide locked ones (except BASIC which is always unlocked). */
    scoreboard_t sb;
    get_scoreboard(&sb);

    const int unlockedN      = scoreboard_is_unlocked(SUITE_NORMAL);
    const int unlockedExt    = scoreboard_is_unlocked(SUITE_EXTERNAL);
    const int unlockedModes  = scoreboard_is_unlocked(SUITE_MODES);
    const int unlockedEdge   = scoreboard_is_unlocked(SUITE_EDGE);
    const int unlockedHidden = scoreboard_is_unlocked(SUITE_HIDDEN);

    /* Build a list of possible choices for user. We'll do a simple numeric menu. */
    /* We'll store { "BASIC", SUITE_BASIC } first, always. Then conditionally others. */
    struct {
        char label[32];
        scoreboard_suite_t suite;
    } items[6];
    int count=0;

    /* 1) BASIC => always visible */
    strcpy(items[count].label, "Basic");
    items[count].suite = SUITE_BASIC;
    count++;

    /* 2) Normal => only if unlocked */
    if(unlockedN) {
        strcpy(items[count].label, "Normal");
        items[count].suite = SUITE_NORMAL;
        count++;
    }
    /* 3) External => only if unlocked */
    if(unlockedExt) {
        strcpy(items[count].label, "External");
        items[count].suite = SUITE_EXTERNAL;
        count++;
    }
    /* 4) Modes => only if unlocked */
    if(unlockedModes) {
        strcpy(items[count].label, "Modes");
        items[count].suite = SUITE_MODES;
        count++;
    }
    /* 5) Edge => only if unlocked */
    if(unlockedEdge) {
        strcpy(items[count].label, "Edge");
        items[count].suite = SUITE_EDGE;
        count++;
    }
    /* 6) Hidden => only if unlocked */
    if(unlockedHidden) {
        strcpy(items[count].label, "Hidden");
        items[count].suite = SUITE_HIDDEN;
        count++;
    }

    if(count==0) {
        printf(CLR_GREEN"\nNo suites are available\n"CLR_RESET);
        pause_enter();
        return;
    }

    printf(CLR_YELLOW"Choose which unlocked suite to run a single test:\n"CLR_RESET);
    for(int i=0; i<count; i++){
        printf(" %d) %s\n", i+1, items[i].label);
    }
    printf(CLR_YELLOW CLR_BOLD"Choice: "CLR_RESET);

    char buf[256];
    if(!read_line(buf, sizeof(buf))) return;
    int pick = parse_int_strtol(buf, -1);
    if(pick<1 || pick>count) {
        printf(CLR_RED"Invalid.\n"CLR_RESET);
        pause_enter();
        return;
    }

    scoreboard_suite_t chosen = items[pick-1].suite;

    /* Now we ask how many tests in that suite => ask user which single test => run. */
    run_single_test_in_suite(chosen); /* from old main.c logic, but we place in runner. */

    pause_enter();
}

/*
   "Run All Unlocked Test Suites" => skip re-running the ones already at 100%
   and skip locked ones.
   We stop if user sends SIGTERM.
*/
void submenu_run_tests(void) {
    /* We'll iterate over the chain in order: BASIC->NORMAL->EXTERNAL->MODES->EDGE->HIDDEN */
    scoreboard_t sb;
    get_scoreboard(&sb);

    printf(CLR_BOLD CLR_CYAN "╔═══════════════════════════════════════════╗\n");
    printf("║     Running all available tests \n");
    printf("╚═══════════════════════════════════════════╝\n" CLR_RESET);

    scoreboard_load(); /* refresh scoreboard just in case */

    scoreboard_suite_t chain[] = {
        SUITE_BASIC,
        SUITE_NORMAL,
        SUITE_EXTERNAL,
        SUITE_MODES,
        SUITE_EDGE,
        SUITE_HIDDEN
    };
    int chain_count = 6;

    for(int i=0; i<chain_count; i++){
        scoreboard_suite_t st = chain[i];
        if(!scoreboard_is_unlocked(st)) {
            /* skip locked */
            continue;
        }


        /* skip if that suite is already at 100% pass. */
        double suite_score=0.0;
        switch(st){
        case SUITE_BASIC:    suite_score = sb.basic_percent;    break;
        case SUITE_NORMAL:   suite_score = sb.normal_percent;   break;
        case SUITE_EXTERNAL: suite_score = sb.external_percent; break;
        case SUITE_MODES:    suite_score = sb.modes_percent;    break;
        case SUITE_EDGE:     suite_score = sb.edge_percent;     break;
        case SUITE_HIDDEN:   suite_score = sb.hidden_percent;   break;
        default: break;
        }
        if(suite_score>=100.0) {
            /* no need to re-run it */
            continue;
        }


        run_entire_suite(st);

        /* check if user wants to do next suite if it got unlocked => handle in attempt_run_next_suite */
        scoreboard_load(); /* refresh scoreboard after the run */
        attempt_run_next_suite(st);
    }

    pause_enter();
}

/*
   Attempt to automatically run next suite if newly unlocked.
   We ask user "do you want to run the next suite immediately (y/n)?"
   If "n", we skip to next.
   If "y", we directly run that suite, then chain further if more is unlocked.
*/
void attempt_run_next_suite(scoreboard_suite_t currentSuite) {
    scoreboard_suite_t chain[] = {
        SUITE_BASIC,
        SUITE_NORMAL,
        SUITE_EXTERNAL,
        SUITE_MODES,
        SUITE_EDGE,
        SUITE_HIDDEN
    };
    int chain_count = 6;

    int found_idx = -1;
    for(int i=0; i<chain_count; i++){
        if(chain[i] == currentSuite){
            found_idx = i;
            break;
        }
    }
    if(found_idx<0 || found_idx==(chain_count-1)) {
        /* Not found, or last suite => no next. */
        return;
    }
    scoreboard_suite_t next = chain[found_idx+1];
    if(!scoreboard_is_unlocked(next)) {
        return; /* not unlocked => do nothing */
    }

    /* So the next suite *is* unlocked => ask user if they want to run it now. */
    printf(CLR_BOLD CLR_GREEN "\n┌───────────────────────────────────────────────────────────┐\n");
    printf("│" CLR_GREEN " Good job! Next suite is now unlocked" CLR_RESET "                │\n");
    printf("│" CLR_GREEN CLR_BOLD " Do you want to run the next suite immediately? (y/n)" CLR_RESET "      │\n");
    printf("└───────────────────────────────────────────────────────────┘\n" CLR_RESET);
    printf(CLR_YELLOW CLR_BOLD"Choice: "CLR_RESET);

    char buf[256];
    if(!read_line(buf, sizeof(buf))) {
        return;
    }
    if(buf[0] == 'y' || buf[0] == 'Y') {
        /* directly run that suite now. */
        run_entire_suite(next);
        /* that might unlock the suite after that => chain again. */
        attempt_run_next_suite(next);
    } else {
        /* skip to next. */
        menu_main_loop();
    }
}

/*
   Submenu for external concurrency (only if EXTERNAL suite is >= 60%).
*/
void menu_submenu_external_concurrency(void) {
    int unlockedExt = scoreboard_is_unlocked(SUITE_EXTERNAL);
    if(!unlockedExt) {
        printf(CLR_BOLD CLR_RED "\n[External Concurrency] is locked (need 60%% pass on EXTERNAL suite).\n" CLR_RESET);
        pause_enter();
        return;
    }

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

    int base=2;
    switch(style) {
        case 1: base=2;  break;  /* short */
        case 2: base=6;  break;  /* medium */
        case 3: base=30; break;  /* stress */
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
        printf(CLR_BOLD CLR_YELLOW "0" CLR_RESET "=" CLR_CYAN "FIRST IN FIRST OUT\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "1" CLR_RESET "=" CLR_CYAN "ROUND ROBIN\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "2" CLR_RESET "=" CLR_CYAN "COMPLETELY FAIR SCHEDULER CFS\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "3" CLR_RESET "=" CLR_CYAN "CFS SHORTEST REMAINING TIME FIRST\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "4" CLR_RESET "=" CLR_CYAN "BRAIN FAIR SCHEDULER\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "5" CLR_RESET "=" CLR_CYAN "SHORTEST JOB FIRST\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "6" CLR_RESET "=" CLR_CYAN "SHORTEST TIME REMAINING FIRST\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "7" CLR_RESET "=" CLR_CYAN "HIGHEST RESPONSE RATIO NEXT\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "8" CLR_RESET "=" CLR_CYAN "HIGHEST RESPONSE RATIO NEXT WITH REMAINING TIME\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "9" CLR_RESET "=" CLR_CYAN "PRIORITY SCHEDULING\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "10" CLR_RESET "=" CLR_CYAN "HIGH PERFORMANCE COMPUTING OVERLAY\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "11" CLR_RESET "=" CLR_CYAN "MULTI-LEVEL FEEDBACK QUEUE\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW"Choice: "CLR_RESET);
        if(!read_line(buf,sizeof(buf))){
            pause_enter();
            for(int i=0;i<n;i++) free(lines[i]);
            free(lines);
            return;
        }
        const int mode = parse_int_strtol(buf, -1);
        if(mode<0 || mode>11){
            printf("Invalid mode.\n");
            pause_enter();
        } else {
            run_shell_commands_concurrently(n, lines, c, mode, 0);
        }
    } else {
        /* run with all modes => pass -1, allModes=1 */
        run_shell_commands_concurrently(n, lines, c, -1, 1);
    }

    for(int i=0; i<n; i++){
        free(lines[i]);
    }
    free(lines);

    pause_enter();
}

/*
   The menu option for external tests => only if EXTERNAL suite is unlocked.
*/
static void menu_run_external_tests(void) {
    if(!scoreboard_is_unlocked(SUITE_EXTERNAL)) {
        printf(CLR_RED"External tests locked\n"CLR_RESET);
        pause_enter();
        return;
    }
    printf(CLR_CYAN"\nRunning external tests\n"CLR_RESET);
    run_external_tests_menu(); /* from runner.c */
    scoreboard_save();
    pause_enter();
}

/*
   The main menu loop
*/
void menu_main_loop(void) {
    while(1){
        int sp = stats_get_speed_mode();
        const char* sp_text = (sp==0) ? "NORMAL" : "FAST";
        printf(CLR_BOLD CLR_YELLOW " ┌────────────────────────────────────────────┐\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │             OS-SCHEDULING GAME             │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " └────────────────────────────────────────────┘\n" CLR_RESET);
        printf(CLR_RED "     A concurrency and scheduling trainer   \n" CLR_RESET);
        printf(CLR_BOLD CLR_RED "          [Current Speed Mode: %s]\n\n" CLR_RESET, sp_text);
        printf(CLR_BOLD CLR_YELLOW " ┌─── MAIN MENU ─────────────────────────────┐\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 1) Run All Unlocked Test Suites           │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 2) Exit                                   │\n" CLR_RESET);
        int unlockedExt = scoreboard_is_unlocked(SUITE_EXTERNAL);
        if(unlockedExt){
            printf(CLR_BOLD CLR_YELLOW " │ 3) External Shell Concurrency DEMO        │\n" CLR_RESET);
        } else {
            printf(CLR_BOLD CLR_GRAY   " │ 3) External Shell Concurrency DEMO (LOCK) │\n" CLR_RESET);
        }

        /* "Run External Tests" => only visible if EXTERNAL is unlocked. */
        if(unlockedExt){
            printf(CLR_BOLD CLR_YELLOW " │ 4) Run External Tests                     │\n" CLR_RESET);
        } else {
            printf(CLR_BOLD CLR_GRAY   " │ 4) Run External Tests (LOCK)              │\n" CLR_RESET);
        }

        printf(CLR_BOLD CLR_YELLOW " │ 5) Show Scoreboard                        │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 6) Clear Scoreboard                       │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 7) Toggle Speed Mode                      │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " │ 8) Run Single Test                        │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW " └───────────────────────────────────────────┘\n\n" CLR_RESET);

        printf(CLR_BOLD CLR_YELLOW"Choice: "CLR_RESET);

        char input[256];
        if(!read_line(input, sizeof(input))){
            menu_main_loop();
        }

        int choice = parse_int_strtol(input, -1);

        switch(choice){
        case 1:
            submenu_run_tests();
            break;
        case 2: {
            /* Exit */
            const int fs = scoreboard_get_final_score();
            printf(CLR_GREEN"\nExiting with final score = %d.\n"CLR_RESET, fs);
            os_cleanup();
            scoreboard_save();
            stats_print_summary();
            exit(fs);
            break;
        }
        case 3:
            /* External concurrency => only if unlocked */
            if(unlockedExt){
                menu_submenu_external_concurrency();
            } else {
                printf(CLR_RED"Locked.\n"CLR_RESET);
                pause_enter();
            }
            break;
        case 4:
            /* Run external tests => only if unlocked */
            if(unlockedExt){
                menu_run_external_tests();
            } else {
                printf(CLR_RED"Locked.\n"CLR_RESET);
                pause_enter();
            }
            break;
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
            printf(CLR_RED"Invalid.\n"CLR_RESET);
            pause_enter();
            break;
        }
    }
}