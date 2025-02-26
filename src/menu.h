#ifndef MENU_H
#define MENU_H
#include "scoreboard.h"
/*
 * menu.h => The main user-interface menu system.
 *           Exposes just the function to start the main loop.
 */

/* Enters the main menu loop (blocking). Never returns unless user exits. */
void menu_main_loop(void);
static void attempt_run_next_suite(scoreboard_suite_t currentSuite);
#endif
