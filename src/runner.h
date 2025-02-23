#ifndef RUNNER_H
#define RUNNER_H

/*
  runner.h => code to run test suites, or concurrency shell commands
  with chosen scheduling modes.
*/

void run_all_levels(void);
void run_external_tests_menu(void);

void run_shell_commands_concurrently(int count,
                                     char** lines,
                                     int coreCount,
                                     int mode,
                                     int allModes);

#endif
