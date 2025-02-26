#include "stats.h"
#include <stdio.h>
#include <string.h>
#include "safe_calls_library.h"

/* Global stats object. */
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

void stats_inc_signal_sigint(void) {
    g_stats.signals_received_sigint++;
}
void stats_inc_signal_sigterm(void) {
    g_stats.signals_received_sigterm++;
}
void stats_inc_signal_other(void) {
    g_stats.signals_received_others++;
}
void stats_inc_concurrency_runs(void) {
    g_stats.concurrency_runs++;
}
void stats_inc_processes_spawned(void) {
    g_stats.processes_spawned++;
}
void stats_inc_containers_created(void) {
    g_stats.containers_created++;
}
void stats_inc_containers_removed(void) {
    g_stats.containers_removed++;
}
void stats_inc_tests_passed(int count) {
    g_stats.tests_passed += count;
}
void stats_inc_tests_failed(int count) {
    g_stats.tests_failed += count;
}
void stats_inc_concurrency_commands(int n) {
    g_stats.concurrency_commands_run += n;
}

/*
  Print final stats with nice ASCII box.
*/
void stats_print_summary(void) {
    printf(CLR_BOLD CLR_CYAN "\n╔═══════════════════ FINAL STATS ═════════════════╗\n");
    printf("║ Speed Mode            : %s\n", g_stats.speed_mode ? "FAST" : "NORMAL");
    printf("║ Signals (SIGINT)      : %d\n", g_stats.signals_received_sigint);
    printf("║ Signals (SIGTERM)     : %d\n", g_stats.signals_received_sigterm);
    printf("║ Signals (Others)      : %d\n", g_stats.signals_received_others);
    printf("║ Concurrency Runs      : %d\n", g_stats.concurrency_runs);
    printf("║ Processes Spawned     : %d\n", g_stats.processes_spawned);
    printf("║ Containers Created    : %d\n", g_stats.containers_created);
    printf("║ Containers Removed    : %d\n", g_stats.containers_removed);
    printf("║ Tests Passed          : %d\n", g_stats.tests_passed);
    printf("║ Tests Failed          : %d\n", g_stats.tests_failed);
    printf("║ Concurrency Cmds Run  : %d\n", g_stats.concurrency_commands_run);
    printf("╚═════════════════════════════════════════════════╝\n" CLR_RESET);
}
