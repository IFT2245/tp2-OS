#include "stats.h"
#include <stdio.h>
#include <string.h>
#include "safe_calls_library.h"

/* Global stats object
 * Support for speed mode
 */
static stats_t g_stats;
void stats_init(void) {
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.speed_mode = 0; /* default => normal speed */
}
void stats_get(stats_t* out) {
    if(out) {
        *out = g_stats;
    }
}
void stats_set_speed_mode(int mode) {
    g_stats.speed_mode = (mode != 0) ? 1 : 0;
}
int stats_get_speed_mode(void) {
    return g_stats.speed_mode;
}


static char g_fail_reason[256] = {0};
void test_set_fail_reason(const char* msg) {
    if(!msg) {
        g_fail_reason[0] = '\0';
        return;
    }
    strncpy(g_fail_reason, msg, sizeof(g_fail_reason)-1);
    g_fail_reason[sizeof(g_fail_reason)-1] = '\0';
}
const char* test_get_fail_reason(void) {
    if(g_fail_reason[0] == '\0') {
        return "???";
    }
    return g_fail_reason;
}
void stats_inc_tests_passed(int count) {
    g_stats.tests_passed = count;
}
void stats_inc_tests_failed(int count) {
    g_stats.tests_failed = count;
}




/*  EACH TEST SHOULD USE A FUNCTION LIKE THIS */
void stats_set_test_fifo(int state) {
    g_stats.test_fifo = state;
}
int stats_get_test_fifo(void) {
    return g_stats.test_fifo;
}




void stats_print_summary(void) {
    printf(CLR_BOLD CLR_CYAN "\n╔═══════════════════ FINAL STATS ═════════════════╗\n");
    printf("║ Speed Mode            : %s\n", g_stats.speed_mode ? "FAST" : "NORMAL");
    printf("║ Tests Passed          : %d\n", g_stats.tests_passed);
    printf("║ Tests Failed          : %d\n", g_stats.tests_failed);
    printf("╚═════════════════════════════════════════════════╝\n" CLR_RESET);
}
