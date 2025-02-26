#ifndef MENU_H
#define MENU_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include "stats.h"
#include "scoreboard.h"
#include "runner.h"
#include "os.h"
#include "safe_calls_library.h"


/*
 * menu.h => The main user-interface menu system.
 *           Exposes just the function to start the main loop.
 */

/* Enters the main menu loop (blocking). Never returns unless user exits. */
void menu_main_loop(void);
static void attempt_run_next_suite(scoreboard_suite_t currentSuite);

/* Clears the terminal screen. */
void clear_screen(void);

/* Pauses until the user presses ENTER. */
void pause_enter(void);

/* Reads one line (up to sz-1 chars). Returns 1 if success, 0 if error/EOF. */
ssize_t read_line(char *buf, size_t sz);

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
