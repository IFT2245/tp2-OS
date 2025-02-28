#include "ephemeral.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_EPHEMERAL_PATH 256

/*
   If you want fully recursive removal,
   define EPHEMERAL_RM_RECURSIVE in the build.
*/
#ifdef EPHEMERAL_RM_RECURSIVE
static int remove_directory_recursive(const char* path){
    DIR* dir = opendir(path);
    if(!dir){
        return rmdir(path);
    }
    struct dirent* entry;
    int ret=0;
    while((entry = readdir(dir))){
        if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0){
            continue;
        }
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);

        struct stat st;
        if(stat(buf, &st)==0){
            if(S_ISDIR(st.st_mode)){
                ret = remove_directory_recursive(buf);
                if(ret!=0) break;
            } else {
                ret = unlink(buf);
                if(ret!=0) break;
            }
        }
    }
    closedir(dir);
    if(ret==0){
        ret = rmdir(path);
    }
    return ret;
}
#endif

char* ephemeral_create_container(void){
    char tmpl[] = "/tmp/container_XXXXXX";
    char* p = (char*)malloc(MAX_EPHEMERAL_PATH);
    if(!p){
        log_error("ephemeral_create_container mem fail");
        return NULL;
    }
    strcpy(p, tmpl);
    if(!mkdtemp(p)){
        log_error("mkdtemp fail %s => %s", p, strerror(errno));
        free(p);
        return NULL;
    }
    log_info("ephemeral created => %s", p);
    return p;
}

void ephemeral_remove_container(const char* path){
    if(!path) return;
#ifdef EPHEMERAL_RM_RECURSIVE
    int r = remove_directory_recursive(path);
#else
    int r = rmdir(path);
#endif
    if(r == 0){
        log_info("ephemeral removed => %s", path);
    } else {
        log_warn("ephemeral remove fail => %s : %s", path, strerror(errno));
    }
}
