#include <stdio.h>
#include "runner.h"
#include "../test/basic-test.h"
#include "../test/normal-test.h"
#include "../test/modes-test.h"
#include "../test/edge-test.h"
#include "../test/hidden-test.h"

static int unlocked_basic    = 1;
static int unlocked_normal   = 0;
static int unlocked_modes    = 0;
static int unlocked_edge     = 0;
static int unlocked_hidden   = 0;

static const double PASS_THRESHOLD = 60.0;

static void show_score(const char* lvl, int total, int passed, int* unlock) {
    if(total <= 0) return;
    double rate = (passed * 100.0) / total;
    printf("[%s] %d tests, %d passed, %.1f%% -> ", lvl, total, passed, rate);
    if(rate >= PASS_THRESHOLD) {
        *unlock = 1;
        printf("Next unlocked\n");
    } else {
        printf("Next locked\n");
    }
}

void run_all_levels(void) {
    int tot=0, pass=0;

    if(unlocked_basic) {
        run_basic_tests(&tot,&pass);
        show_score("BASIC",tot,pass,&unlocked_normal);
    }
    if(unlocked_normal) {
        tot=0; pass=0;
        run_normal_tests(&tot,&pass);
        show_score("NORMAL",tot,pass,&unlocked_modes);
    }
    if(unlocked_modes) {
        tot=0; pass=0;
        run_modes_tests(&tot,&pass);
        show_score("MODES",tot,pass,&unlocked_edge);
    }
    if(unlocked_edge) {
        tot=0; pass=0;
        run_edge_tests(&tot,&pass);
        show_score("EDGE",tot,pass,&unlocked_hidden);
    }
    if(unlocked_hidden) {
        tot=0; pass=0;
        run_hidden_tests(&tot,&pass);
        double r = (pass*100.0)/tot;
        printf("[HIDDEN] %d tests, %d passed, %.1f%%\n", tot, pass, r);
    }
}
