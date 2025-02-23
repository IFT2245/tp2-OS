#include "test_common.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* global reason for test failure */
char g_test_fail_reason[256] = {0};

/*
  read_all():
    Reads from fd until EOF, storing in buf up to cap-1 bytes.
    Null-terminates.
*/
static ssize_t read_all(int fd, char* buf, size_t cap){
    size_t used=0;
    while(used + 1 < cap){
        ssize_t r=read(fd, buf + used, cap - 1 - used);
        if(r<0 && errno==EINTR){
            continue;
        }
        if(r<=0){
            break;
        }
        used+=(size_t)r;
    }
    buf[used] = '\0';
    return (ssize_t)used;
}

/*
  run_function_capture_output():
    1) Create pipes for stdout & stderr.
    2) Fork => child => redirect to pipes => run test function.
    3) Parent => read from pipes => store in out->stdout_buf/stderr_buf.
    4) Wait for child => return exit status code.
*/
int run_function_capture_output(void(*fn)(void), struct captured_output* out){
    if(!fn || !out){
        return -1;
    }
    int p_out[2], p_err[2];
    if(pipe(p_out)==-1 || pipe(p_err)==-1){
        return -1;
    }
    int save_out=dup(STDOUT_FILENO);
    int save_err=dup(STDERR_FILENO);
    if(save_out<0||save_err<0){
        return -1;
    }

    pid_t c=fork();
    if(c<0){
        return -1; // fork fail
    }
    if(c==0){
        // child
        close(p_out[0]);
        close(p_err[0]);
        dup2(p_out[1],STDOUT_FILENO);
        dup2(p_err[1],STDERR_FILENO);
        close(p_out[1]);
        close(p_err[1]);

        fn(); // run test function in child
        _exit(0);
    }

    // parent
    close(p_out[1]);
    close(p_err[1]);
    read_all(p_out[0], out->stdout_buf, sizeof(out->stdout_buf));
    read_all(p_err[0], out->stderr_buf, sizeof(out->stderr_buf));
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
