#include "worker.h"
#include "stats.h"
#include <stdio.h>
#include <unistd.h>

/*
  We drastically reduce real-time sleeps to ensure that
  in FAST mode, all tests can finish under ~5 seconds total.

  We'll define a new scaling:
    - In NORMAL mode: usleep(slice_ms * 20000)
    - In FAST mode:   usleep(slice_ms * 2000)
  This is a ~10x difference. That should keep normal mode somewhat slow
  and fast mode quite quick.
*/

void simulate_process_partial(process_t* p, unsigned long slice_ms, int core_id) {
    if (!p || slice_ms == 0) return;

    if(stats_get_speed_mode() == 0) {
        /* NORMAL mode => bigger sleep for user-friendly pacing */
        usleep((useconds_t)(slice_ms * 20000U));
    } else {
        /* FAST mode => short sleep to finish quickly slice_ms = 1/50 */
        usleep((useconds_t)(slice_ms * 2000U));
    }
}
