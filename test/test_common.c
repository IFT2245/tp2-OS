#include "test_common.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

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

static ssize_t read_all_fd(int fd, char* buf, size_t cap) {
    size_t used = 0;
    while(used + 1 < cap) {
        ssize_t r = read(fd, buf + used, cap - 1 - used);
        if(r < 0 && errno == EINTR) continue;
        if(r <= 0) break;
        used += (size_t)r;
    }
    buf[used] = '\0';
    return (ssize_t)used;
}

int run_function_capture_output(void(*fn)(void), struct captured_output* out) {
    if(!fn || !out) return -1;
    int p_out[2], p_err[2];
    if(pipe(p_out)==-1 || pipe(p_err)==-1) return -1;
    int save_out = dup(STDOUT_FILENO);
    int save_err = dup(STDERR_FILENO);
    if(save_out<0 || save_err<0) return -1;

    pid_t c = fork();
    if(c<0) {
        return -1;
    }
    if(c==0) {
        close(p_out[0]);
        close(p_err[0]);
        dup2(p_out[1], STDOUT_FILENO);
        dup2(p_err[1], STDERR_FILENO);
        close(p_out[1]);
        close(p_err[1]);

        fn();
        _exit(0);
    } else {
        close(p_out[1]);
        close(p_err[1]);
        read_all_fd(p_out[0], out->stdout_buf, sizeof(out->stdout_buf));
        read_all_fd(p_err[0], out->stderr_buf, sizeof(out->stderr_buf));
        close(p_out[0]);
        close(p_err[0]);

        dup2(save_out, STDOUT_FILENO);
        dup2(save_err, STDERR_FILENO);
        close(save_out);
        close(save_err);

        int st=0;
        waitpid(c,&st,0);
        return st;
    }
    return 0;
}
