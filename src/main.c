#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "runner.h"
#include "os.h"
#include "safe_calls_library.h"

/* Colors and styles */
#define CLR_RESET     "\033[0m"
#define CLR_CYAN      "\033[96m"
#define CLR_RED       "\033[91m"
#define CLR_YELLOW    "\033[93m"
#define CLR_GRAY      "\033[90m"
#define CLR_GREEN     "\033[92m"
#define CLR_MAGENTA   "\033[95m"
#define CLR_BOLD      "\033[1m"

/* Global unlock flags managed by runner.c scoreboard logic */

extern int unlocked_basic;
extern int unlocked_normal;
extern int unlocked_external;
extern int unlocked_modes;
extern int unlocked_edge;
extern int unlocked_hidden;

/* Forward declarations */
static void clear_screen(void);
static int  read_line(char *buf, size_t sz);
static void menu_show_scoreboard(void);

/* Clears screen for cross-platform */
static void clear_screen(void){
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

/* Reads one line into buf; returns 1 if not empty, 0 if EOF or empty. */
static int read_line(char *buf, size_t sz){
    if(!fgets(buf, sz, stdin)) return 0;
    buf[strcspn(buf, "\n")] = '\0';
    return (*buf != '\0');
}

/* Displays scoreboard with color-coded results and unlock info. */
static void menu_show_scoreboard(void){
    /* We fetch scoreboard info from runner. */
    scoreboard_t sb;
    get_scoreboard(&sb);

    clear_screen();
    printf(CLR_BOLD CLR_MAGENTA "================== SCOREBOARD ==================\n" CLR_RESET);
    printf(" Levels & Tests Status:\n\n");

    /* Basic */
    printf("  BASIC   => %d/%d passed (%.1f%%) => ",
           sb.basic_pass, sb.basic_total, sb.basic_percent);
    if(sb.basic_percent >= sb.pass_threshold) printf(CLR_GREEN "UNLOCKED" CLR_RESET);
    else                                      printf(CLR_RED   "LOCKED"   CLR_RESET);
    printf("\n");

    /* Normal */
    printf("  NORMAL  => %d/%d passed (%.1f%%) => ",
           sb.normal_pass, sb.normal_total, sb.normal_percent);
    if(sb.normal_percent >= sb.pass_threshold) printf(CLR_GREEN "UNLOCKED" CLR_RESET);
    else                                       printf(CLR_RED   "LOCKED"   CLR_RESET);
    printf("\n");

    /* MODES */
    printf("  MODES   => %d/%d passed (%.1f%%) => ",
           sb.modes_pass, sb.modes_total, sb.modes_percent);
    if(sb.modes_percent >= sb.pass_threshold) printf(CLR_GREEN "UNLOCKED" CLR_RESET);
    else                                      printf(CLR_RED   "LOCKED"   CLR_RESET);
    printf("\n");

    /* EDGE */
    printf("  EDGE    => %d/%d passed (%.1f%%) => ",
           sb.edge_pass, sb.edge_total, sb.edge_percent);
    if(sb.edge_percent >= sb.pass_threshold) printf(CLR_GREEN "UNLOCKED" CLR_RESET);
    else                                     printf(CLR_RED   "LOCKED"   CLR_RESET);
    printf("\n");

    /* HIDDEN */
    printf("  HIDDEN  => %d/%d passed (%.1f%%) => ",
           sb.hidden_pass, sb.hidden_total, sb.hidden_percent);
    if(sb.hidden_percent >= sb.pass_threshold) printf(CLR_GREEN "UNLOCKED" CLR_RESET);
    else                                       printf(CLR_RED   "LOCKED"   CLR_RESET);
    printf("\n");

    /* External */
    printf("  EXTERNAL => forcibly passes each sub-run, but overall => ");
    if(unlocked_external) printf(CLR_GREEN "UNLOCKED" CLR_RESET);
    else                  printf(CLR_RED   "LOCKED"   CLR_RESET);
    printf("\n");

    /* Show thresholds and final notes */
    printf("\nPass threshold: %.1f%%\n", sb.pass_threshold);
    printf("Unlocked so far => BASIC:%d, NORMAL:%d, MODES:%d, EDGE:%d, HIDDEN:%d, EXTERNAL:%d\n",
           unlocked_basic, unlocked_normal, unlocked_modes, unlocked_edge, unlocked_hidden, unlocked_external);
    printf(CLR_MAGENTA "=================================================\n" CLR_RESET);
    printf("\nPress ENTER to return to main menu...");
    char tmp[256];
    read_line(tmp, sizeof(tmp));
}

int main(int argc, char** argv){
    (void)argc; (void)argv;
    os_init();

    for(;;){
        clear_screen();
        printf(CLR_YELLOW "========================================\n");
        printf("              TP2-OS Menu\n");
        printf("========================================" CLR_RESET "\n");
        printf("1) Run All Unlocked Levels\n");
        printf("2) Exit\n");
        printf("3) External Shell Concurrency\n");
        if(unlocked_external){
            printf("4) Run External Scheduling Tests\n");
        } else {
            printf("4) Run External Scheduling Tests " CLR_GRAY "(locked)" CLR_RESET "\n");
        }
        printf("5) Show Scoreboard\n");

        printf("\nSelect an option: ");
        fflush(stdout);

        char input[256];
        if(!read_line(input, sizeof(input))){
            printf("\nNo input. Exiting.\n");
            os_cleanup();
            return 0;
        }
        int choice = parse_int_strtol(input, -1);

        switch(choice){
        case 1: {
            printf("\n" CLR_CYAN "[main]" CLR_RESET " Running all unlocked levels...\n\n");
            run_all_levels();
            /* Pause for user input but avoid double enter bug by cleaning one extra line. */
            printf("\nPress ENTER to continue...");
            read_line(input, sizeof(input));
        } break;

        case 2:
            printf("\n" CLR_CYAN "[main]" CLR_RESET " Exiting...\n");
            os_cleanup();
            return 0;

        case 3: {
            printf("\nHow many shell processes to start concurrently? ");
            if(!read_line(input, sizeof(input))) break;
            int count = parse_int_strtol(input, 0);
            if(count <= 0){
                printf(CLR_RED "Invalid concurrency count." CLR_RESET "\nPress ENTER...");
                read_line(input, sizeof(input));
                break;
            }
            printf("How many CPU cores to simulate? (default=2) ");
            if(!read_line(input, sizeof(input))) break;
            int coreCount = parse_int_strtol(input, 2);
            if(coreCount < 1) coreCount = 2;

            char** lines = (char**)calloc((size_t)count, sizeof(char*));
            if(!lines){
                printf(CLR_RED "Memory allocation failed." CLR_RESET "\nPress ENTER...");
                read_line(input, sizeof(input));
                break;
            }
            for(int i=0; i<count; i++){
                printf("Command line for process #%d: ", i+1);
                char buf[256];
                if(!read_line(buf, sizeof(buf))) buf[0] = '\0';
                lines[i] = strdup(buf);
            }
            run_shell_commands_concurrently(count, lines, coreCount);

            for(int i=0; i<count; i++){
                free(lines[i]);
            }
            free(lines);
            /* Return silently to main menu, no extra 'Press ENTER'. */
        } break;

        case 4:
            if(unlocked_external){
                printf("\n" CLR_CYAN "[main]" CLR_RESET " Running external scheduling tests...\n\n");
                run_external_tests_menu();
            } else {
                printf("\n" CLR_YELLOW "External tests locked. Complete BASIC >=60%% first." CLR_RESET "\n");
            }
            printf("\nPress ENTER to continue...");
            read_line(input, sizeof(input));
            break;

        case 5:
            menu_show_scoreboard();
            break;

        default:
            printf("\n" CLR_RED "Invalid input." CLR_RESET "\nPress ENTER to continue...");
            read_line(input, sizeof(input));
            break;
        }
    }
    os_cleanup();
    return 0;
}
