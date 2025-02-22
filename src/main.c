#include <stdio.h>
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

static void clear_screen(void){
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif
}

static void pause_enter(void){
    printf("\nPress ENTER...");
    fflush(stdout);
    int c; while((c=getchar())!=EOF && c!='\n'){}
}

static int read_line(char *buf, size_t sz){
    if(!fgets(buf, sz, stdin)) return 0;
    buf[strcspn(buf, "\n")] = '\0';
    return *buf != '\0';
}

static void menu_show_scoreboard(void){
    scoreboard_t sb;
    get_scoreboard(&sb);
    clear_screen();
    printf(CLR_BOLD CLR_MAGENTA "=== SCOREBOARD (SQLite) ===\n" CLR_RESET);
    printf(" BASIC    => %d/%d (%.1f%%) => %s\n", sb.basic_pass, sb.basic_total, sb.basic_percent, unlocked_basic?CLR_GREEN"UNLOCKED"CLR_RESET:CLR_RED"LOCKED"CLR_RESET);
    printf(" NORMAL   => %d/%d (%.1f%%) => %s\n", sb.normal_pass, sb.normal_total, sb.normal_percent, unlocked_normal?CLR_GREEN"UNLOCKED"CLR_RESET:CLR_RED"LOCKED"CLR_RESET);
    printf(" MODES    => %d/%d (%.1f%%) => %s\n", sb.modes_pass, sb.modes_total, sb.modes_percent, unlocked_modes?CLR_GREEN"UNLOCKED"CLR_RESET:CLR_RED"LOCKED"CLR_RESET);
    printf(" EDGE     => %d/%d (%.1f%%) => %s\n", sb.edge_pass, sb.edge_total, sb.edge_percent, unlocked_edge?CLR_GREEN"UNLOCKED"CLR_RESET:CLR_RED"LOCKED"CLR_RESET);
    printf(" HIDDEN   => %d/%d (%.1f%%) => %s\n", sb.hidden_pass, sb.hidden_total, sb.hidden_percent, unlocked_hidden?CLR_GREEN"UNLOCKED"CLR_RESET:CLR_RED"LOCKED"CLR_RESET);
    printf(" EXTERNAL => %s\n", unlocked_external?CLR_GREEN"UNLOCKED"CLR_RESET:CLR_RED"LOCKED"CLR_RESET);
    printf("\nSchedulers:\n");
    printf(" FIFO:%s RR:%s CFS:%s CFS-SRTF:%s BFS:%s SJF:%s STRF:%s HRRN:%s HRRN-RT:%s PRIORITY:%s HPC-OVER:%s MLFQ:%s\n",
           sb.sc_fifo?"✔":"✘", sb.sc_rr?"✔":"✘", sb.sc_cfs?"✔":"✘", sb.sc_cfs_srtf?"✔":"✘",
           sb.sc_bfs?"✔":"✘", sb.sc_sjf?"✔":"✘", sb.sc_strf?"✔":"✘", sb.sc_hrrn?"✔":"✘",
           sb.sc_hrrn_rt?"✔":"✘", sb.sc_priority?"✔":"✘", sb.sc_hpc_over?"✔":"✘", sb.sc_mlfq?"✔":"✘");
    pause_enter();
}

static void menu_clear_scoreboard(void){
    scoreboard_clear();
    printf("\nCleared.\n");
    pause_enter();
}

int main(int argc, char** argv){
    (void)argc;(void)argv;
    scoreboard_init_db();
    scoreboard_load();
    os_init();

    for(;;){
        clear_screen();
        printf(CLR_YELLOW "===== MENU =====\n" CLR_RESET);
        printf("1) Run All Unlocked\n");
        printf("2) Exit\n");
        printf("3) External Shell Concurrency\n");
        if(unlocked_external) printf("4) External Tests\n"); else printf("4) External Tests " CLR_GRAY"(locked)"CLR_RESET"\n");
        printf("5) Show Scoreboard\n");
        printf("6) Clear Scoreboard\n");
        printf("\nOption: ");
        char input[256]; if(!read_line(input,sizeof(input))) {
            printf("Exiting.\n");
            os_cleanup();
            scoreboard_save();
            scoreboard_close_db();
            return 0;
        }
        int choice = parse_int_strtol(input,-1);
        switch(choice){
        case 1:
            printf(CLR_CYAN "\nRunning all unlocked...\n" CLR_RESET);
            run_all_levels();
            scoreboard_save();
            pause_enter();
            break;
        case 2:
            printf("\nExiting.\n");
            os_cleanup();
            scoreboard_save();
            scoreboard_close_db();
            return 0;
        case 3:{
            printf("How many concurrent shells? ");
            if(!read_line(input,sizeof(input))) break;
            int n = parse_int_strtol(input,0);
            if(n<1){ printf("Invalid.\n"); pause_enter(); break; }
            printf("How many CPU cores? ");
            if(!read_line(input,sizeof(input))) break;
            int c = parse_int_strtol(input,2); if(c<1) c=2;
            char** lines = (char**)calloc(n,sizeof(char*));
            for(int i=0;i<n;i++){
                printf("Command #%d: ",i+1);
                char buf[256]; if(!read_line(buf,sizeof(buf))) buf[0]='\0';
                lines[i] = strdup(buf);
            }
            run_shell_commands_concurrently(n,lines,c);
            for(int i=0;i<n;i++) free(lines[i]);
            free(lines);
            pause_enter();
            break;
        }
        case 4:
            if(!unlocked_external){
                printf("Locked.\n");
                pause_enter();
            } else {
                printf("\nRunning external...\n");
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
}
