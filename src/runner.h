#ifndef RUNNER_H
#define RUNNER_H

void run_all_levels(void);
void run_external_tests_menu(void);

/*
 run_shell_commands_concurrently():
  - If mode<0 & allModes=1 => test all known sched modes
  - Else => test the chosen mode
  - Single-line concurrency timeline per core
*/
void run_shell_commands_concurrently(int count,char** lines,int coreCount,int mode,int allModes);

#endif
