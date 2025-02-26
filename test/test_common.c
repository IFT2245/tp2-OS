#include "test_common.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

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