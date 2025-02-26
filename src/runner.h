#ifndef RUNNER_H
#define RUNNER_H

#include "scoreboard.h"

/*
  runner.h => concurrency logic & test-suites runner utilities
*/

/* Runs the entire suite (all tests in that suite) and updates scoreboard. */
void run_entire_suite(scoreboard_suite_t suite);

/* Runs external tests (if unlocked) -> updates scoreboard. */
void run_external_tests_menu(void);

/*
  Runs a single test from a chosen suite index => scoreboard updated with +1 test, +1 pass if success.
*/
void run_single_test_in_suite(scoreboard_suite_t suite);

/*
  Concurrency function: run shell commands concurrently under a single or all scheduling modes.
   - count, lines => list of commands
   - coreCount => how many CPU cores
   - mode => which single scheduling mode or -1 for all
   - allModes => 1 => do all from 0..11, 0 => do just 'mode'
*/
void run_shell_commands_concurrently(int count,
                                     char** lines,
                                     int coreCount,
                                     int mode,
                                     int allModes);

#endif
