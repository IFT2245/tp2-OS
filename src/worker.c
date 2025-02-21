#include "worker.h"
#include <unistd.h>
#include "os.h"

void simulate_process(process_t* p) {
    if(!p) return;
    usleep((useconds_t)(p->burst_time*1000));
}
