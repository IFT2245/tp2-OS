/******************************************************************************
 * main.c - Entry point with maximal effort on concurrency display and tests. *
 ******************************************************************************/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "runner.h"
#include "os.h"
#include "safe_calls_library.h"
#include "scoreboard.h"

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_MAGENTA "\033[95m"
#define CLR_RED     "\033[91m"
#define CLR_GRAY    "\033[90m"
#define CLR_GREEN   "\033[92m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"

/* Forward declarations */
static void cleanup_and_exit(int code);
static void handle_signal(int signum);

/* Simple terminal screen clear. */
static void clear_screen(void){
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

/* Press ENTER to continue. */
static void pause_enter(void){
    printf("\nPress ENTER...");
    fflush(stdout);
    int c;
    while((c = getchar()) != '\n' && c != EOF){}
}

/* Safe read_line for user input. */
static int read_line(char *buf, size_t sz){
    if(!fgets(buf, sz, stdin)) return 0;
    buf[strcspn(buf, "\n")] = '\0';
    return 1;
}

/* Show scoreboard details with color-coded unlocked status. */
static void menu_show_scoreboard(void){
    scoreboard_t sb;
    get_scoreboard(&sb);
    clear_screen();
    printf(CLR_BOLD CLR_MAGENTA "╔════════════════════════════════════╗\n" CLR_RESET);
    printf(CLR_BOLD CLR_MAGENTA "║           ★ SCOREBOARD ★          ║\n" CLR_RESET);
    printf("║ BASIC       => %.1f/100 => %s\n",
           sb.basic_percent,
           unlocked_basic ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);
    printf("║ NORMAL      => %.1f/100 => %s\n",
           sb.normal_percent,
           unlocked_normal ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);
    printf("║ EXTERNAL    => %.1f/100 => %s\n",
           sb.external_percent,
           unlocked_external ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);
    printf("║ MODES       => %.1f/100 => %s\n",
           sb.modes_percent,
           unlocked_modes ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);
    printf("║ EDGE        => %.1f/100 => %s\n",
           sb.edge_percent,
           unlocked_edge ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);
    printf("║ HIDDEN      => %.1f/100 => %s\n",
           sb.hidden_percent,
           unlocked_hidden ? CLR_GREEN"UNLOCKED"CLR_RESET : CLR_RED"LOCKED"CLR_RESET);
    printf("║\n");
    printf("║ Schedulers mastered:\n");
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
    int final_score = scoreboard_get_final_score();
    printf("║\n");
    printf("╚═ Overall Score => %d/100\n", final_score);
    pause_enter();
}

/* Clear scoreboard (reset all stats). */
static void menu_clear_scoreboard(void){
    scoreboard_clear();
    printf("\nScoreboard cleared.\n");
    pause_enter();
}

/* Safe cleanup => call once at end. */
static void cleanup_and_exit(int code){
    os_cleanup();
    scoreboard_save();
    scoreboard_close();
    exit(code);
}

/* Signal handler => handle SIGINT gracefully, no ignoring. */
static void handle_signal(int signum){
    if(signum == SIGINT){
        printf("\nCaught SIGINT! Saving scoreboard...\n");
        int fs = scoreboard_get_final_score();
        cleanup_and_exit(fs);
    }
    /* We do NOT ignore SIGPIPE or any other signals here to show "maximal effort"
       in gracefully handling concurrency. If a SIGPIPE occurs, we let the default
       behavior happen or user can handle it outside if desired. */
}

/* Sub-menu for concurrency style. */
static int menu_choose_concurrency_level(void){
    printf("\nChoose concurrency test type:\n");
    printf(" 1) Short test (sleep 2)\n");
    printf(" 2) Medium test (sleep 5, etc.)\n");
    printf(" 3) Stress test (sleep 10 or 12, etc.)\n");
    printf("Choice: ");
    char buf[256];
    if(!read_line(buf,sizeof(buf))) return 1;
    int x=parse_int_strtol(buf,1);
    if(x<1 || x>3) x=1;
    return x;
}

/* Sub-menu for external concurrency => single or all modes. */
static void menu_submenu_external_concurrency(void){
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

    /* Build commands array based on style or ALL modes. */
    char** lines = (char**)calloc(n, sizeof(char*));
    if(sub==1){
        int style=menu_choose_concurrency_level();
        if(style==1){
            for(int i=0;i<n;i++){
                char tmp[64];
                snprintf(tmp,sizeof(tmp),"sleep %d", (i+1)*2);
                lines[i]=strdup(tmp);
            }
        } else if(style==2){
            for(int i=0;i<n;i++){
                char tmp[64];
                snprintf(tmp,sizeof(tmp),"sleep %d", ((i+1)*3)+2);
                lines[i]=strdup(tmp);
            }
        } else {
            for(int i=0;i<n;i++){
                char tmp[64];
                snprintf(tmp,sizeof(tmp),"sleep %d", (i+1)*5);
                lines[i]=strdup(tmp);
            }
        }
    } else {
        /* sub==2 => run concurrency with ALL scheduling modes => lines can be uniform. */
        for(int i=0;i<n;i++){
            char tmp[64];
            snprintf(tmp,sizeof(tmp),"sleep %d", (i+1)*2);
            lines[i]=strdup(tmp);
        }
    }

    if(sub==1){
        /* Single mode => ask user for which mode. */
        printf("\nSelect scheduling mode:\n");
        printf(" 0=FIFO,1=RR,2=CFS,3=CFS-SRTF,4=BFS,\n");
        printf(" 5=SJF,6=STRF,7=HRRN,8=HRRN-RT,\n");
        printf(" 9=PRIORITY,10=HPC-OVER,11=MLFQ\n");
        printf("Choice: ");
        if(!read_line(buf,sizeof(buf))){
            pause_enter();
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
        /* sub==2 => run concurrency with all modes. */
        run_shell_commands_concurrently(n, lines, c, -1, 1);
    }

    for(int i=0; i<n; i++){
        free(lines[i]);
    }
    free(lines);
    pause_enter();
}

int main(int argc, char** argv){
    (void)argc; (void)argv;
    signal(SIGINT, handle_signal);
    scoreboard_init();
    scoreboard_load();
    os_init();

    while(1){
        clear_screen();
        printf(CLR_BOLD CLR_YELLOW "┌─── MAIN MENU ────────────────────┐\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 1) Run All Unlocked              │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 2) Exit                          │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 3) External Shell Concurrency    │\n" CLR_RESET);
        if(unlocked_external){
            /* Freed by scoreboard if basic tests pass threshold => external unlocked. */
            printf(CLR_BOLD CLR_YELLOW "│ 4) External Tests                │\n" CLR_RESET);
        } else {
            printf(CLR_BOLD CLR_YELLOW "│ 4) External Tests " CLR_GRAY "(locked)" CLR_RESET "\n");
        }
        printf(CLR_BOLD CLR_YELLOW "│ 5) Show Scoreboard               │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "│ 6) Clear Scoreboard              │\n" CLR_RESET);
        printf(CLR_BOLD CLR_YELLOW "└───────────────────────────────────┘\n\n" CLR_RESET);
        printf("Choice: ");

        char input[256];
        if(!read_line(input, sizeof(input))){
            printf("Exiting (EOF or read error).\n");
            int fs = scoreboard_get_final_score();
            cleanup_and_exit(fs);
        }
        int choice = parse_int_strtol(input, -1);

        switch(choice){
        case 1:
            printf("\n" CLR_CYAN "Running all unlocked tests...\n" CLR_RESET);
            run_all_levels();  /* in runner.c => run each suite if unlocked */
            scoreboard_save();
            pause_enter();
            break;
        case 2:{
            int fs = scoreboard_get_final_score();
            printf("\nExiting with final score = %d.\n", fs);
            cleanup_and_exit(fs);
            break;
        }
        case 3:
            if(!unlocked_external){
                printf("External is locked.\n");
                pause_enter();
            } else {
                menu_submenu_external_concurrency();
            }
            break;
        case 4:
            if(!unlocked_external){
                printf("External tests locked.\n");
                pause_enter();
            } else {
                printf("\nRunning external tests...\n");
                run_external_tests_menu();
                scoreboard_save();
                pause_enter();
            }
            break;
        case 5:
            menu_show_scoreboard();
            break;
        case 6:
            menu_clear_scoreboard();
            break;
        default:
            printf("Invalid.\n");
            pause_enter();
            break;
        }
    }
    return 0;
}
