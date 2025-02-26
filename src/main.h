#ifndef MAIN_H
#define MAIN_H

#include <stddef.h>

/*
 * main.h => Declarations for menu and utility functions used by main.c
 */

/* Cleanup function that saves scoreboard, prints stats, and exits the program. */
void cleanup_and_exit(int code);

/* Signal handler for SIGINT, SIGTERM, SIGUSR1. */
void handle_signal(int signum);

/* Clears the terminal screen. */
void clear_screen(void);

/* Pauses until the user presses ENTER. */
void pause_enter(void);

/* Reads one line (up to sz-1 chars). Returns 1 if success, 0 if error/EOF. */
int read_line(char *buf, size_t sz);

/* Prints the ASCII art main menu header (includes speed mode info). */
void ascii_main_menu_header(void);

/* Submenu to run all unlocked tests (skipping already 100% passed). */
void submenu_run_tests(void);

/* Submenu to run exactly one test from a chosen suite. */
void submenu_run_single_test(void);

/* Shows the scoreboard in a "squared ASCII" format with progress bar. */
void menu_show_scoreboard(void);

/* Clears the scoreboard. */
void menu_clear_scoreboard(void);

/* Toggles speed mode: 0 => normal, 1 => fast. */
void menu_toggle_speed_mode(void);

/* External concurrency submenu. */
void menu_submenu_external_concurrency(void);

#endif
