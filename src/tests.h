#ifndef TESTS_H
#define TESTS_H
#include "scoreboard.h"
#include "log.h"
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "container.h"
/**
 * @brief Run all test cases.
 */
void run_all_tests(void);

#endif
