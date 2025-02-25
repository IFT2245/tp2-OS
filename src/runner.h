#ifndef RUNNER_H
#define RUNNER_H

/*
  runner.h => code to run entire test suites or concurrency commands
              with chosen scheduling modes.
*/

#include <stddef.h>

/* Deprecated stub, replaced by menu logic. */
void run_all_levels(void);

/* Runs the external tests menu, checks scoreboard, etc. */
void run_external_tests_menu(void);

/*
  Run shell commands concurrently with a chosen scheduling mode or all modes.
   - count        => how many commands
   - lines        => array of string commands
   - coreCount    => how many CPU cores
   - mode         => which mode to use; if -1 => run all
   - allModes     => 1 => run all modes, 0 => single mode only
*/
void run_shell_commands_concurrently(int count,
                                     char** lines,
                                     int coreCount,
                                     int mode,
                                     int allModes);

#endif
