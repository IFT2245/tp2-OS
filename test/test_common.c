#include "test_common.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

static ssize_t read_all(int fd, char* buf, size_t cap){
    size_t used=0;
    while(used+1<cap){
        ssize_t r=read(fd,buf+used,cap-1-used);
        if(r<0 && errno==EINTR) continue;
        if(r<=0) break;
        used+=(size_t)r;
    }
    buf[used]='\0';
    return (ssize_t)used;
}

int run_function_capture_output(void(*fn)(void), struct captured_output* out){
    if(!fn||!out)return -1;
    int p_out[2],p_err[2];
    if(pipe(p_out)==-1||pipe(p_err)==-1)return -1;
    int orig_out=dup(STDOUT_FILENO),orig_err=dup(STDERR_FILENO);
    if(orig_out<0||orig_err<0){
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        return -1;
    }
    pid_t c=fork();
    if(c<0){
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        return -1;
    }
    if(c==0){
        dup2(p_out[1],STDOUT_FILENO);
        dup2(p_err[1],STDERR_FILENO);
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        fn();
        _exit(0);
    }
    close(p_out[1]); close(p_err[1]);
    read_all(p_out[0], out->stdout_buf, sizeof(out->stdout_buf));
    read_all(p_err[0], out->stderr_buf, sizeof(out->stderr_buf));
    close(p_out[0]); close(p_err[0]);
    dup2(orig_out,STDOUT_FILENO);
    dup2(orig_err,STDERR_FILENO);
    close(orig_out); close(orig_err);
    int status=0; waitpid(c,&status,0);
    return status;
}

int run_shell_command_capture_output(const char* line, struct captured_output* out){
    if(!line||!out)return -1;
    int p_out[2],p_err[2],p_in[2];
    if(pipe(p_out)==-1||pipe(p_err)==-1||pipe(p_in)==-1)return -1;
    int o_out=dup(STDOUT_FILENO),o_err=dup(STDERR_FILENO),o_in=dup(STDIN_FILENO);
    if(o_out<0||o_err<0||o_in<0){
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        close(p_in[0]);close(p_in[1]);
        return -1;
    }
    pid_t c=fork();
    if(c<0){
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        close(p_in[0]);close(p_in[1]);
        return -1;
    }
    if(c==0){
        dup2(p_in[0],STDIN_FILENO);
        dup2(p_out[1],STDOUT_FILENO);
        dup2(p_err[1],STDERR_FILENO);
        close(p_in[1]);
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        execl("./shell-tp1-implementation","shell-tp1-implementation",(char*)NULL);
        _exit(127);
    }
    close(p_in[0]);
    write(p_in[1],line,strlen(line));
    write(p_in[1],"\n",1);
    close(p_in[1]);
    close(p_out[1]); close(p_err[1]);
    read_all(p_out[0],out->stdout_buf,sizeof(out->stdout_buf));
    read_all(p_err[0],out->stderr_buf,sizeof(out->stderr_buf));
    close(p_out[0]); close(p_err[0]);
    dup2(o_out,STDOUT_FILENO); dup2(o_err,STDERR_FILENO); dup2(o_in,STDIN_FILENO);
    close(o_out);close(o_err);close(o_in);
    int status=0; waitpid(c,&status,0);
    return status;
}

int run_command_capture_output(char* const argv[], struct captured_output* out){
    if(!argv||!argv[0]||!out)return -1;
    int p_out[2],p_err[2];
    if(pipe(p_out)==-1||pipe(p_err)==-1)return -1;
    int o_out=dup(STDOUT_FILENO),o_err=dup(STDERR_FILENO);
    if(o_out<0||o_err<0){
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        return -1;
    }
    pid_t c=fork();
    if(c<0){
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        return -1;
    }
    if(c==0){
        dup2(p_out[1],STDOUT_FILENO);
        dup2(p_err[1],STDERR_FILENO);
        close(p_out[0]);close(p_out[1]);
        close(p_err[0]);close(p_err[1]);
        execvp(argv[0],argv);
        _exit(127);
    }
    close(p_out[1]); close(p_err[1]);
    read_all(p_out[0],out->stdout_buf,sizeof(out->stdout_buf));
    read_all(p_err[0],out->stderr_buf,sizeof(out->stderr_buf));
    close(p_out[0]); close(p_err[0]);
    dup2(o_out,STDOUT_FILENO); dup2(o_err,STDERR_FILENO);
    close(o_out); close(o_err);
    int status=0; waitpid(c,&status,0);
    return status;
}
