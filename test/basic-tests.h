#ifndef TESTS_H
#define TESTS_H

#include "../lib/scoreboard.h"
#include "../lib/log.h"
#include "../src/container.h"
#include <stdbool.h>

/**
 * @brief Runs all tests (BASIC, NORMAL, EDGE, HIDDEN, WFQ, etc.)
 *        and optionally the BONUS HPC BFS test if set.
 */
void run_all_tests(void);

#endif
