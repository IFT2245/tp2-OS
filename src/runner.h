#ifndef RUNNER_H
#define RUNNER_H

/*
  runner.h => code to run entire test suites or concurrency commands
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
