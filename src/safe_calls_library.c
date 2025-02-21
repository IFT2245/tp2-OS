#include "safe_calls_library.h"
#include <pthread.h>
#include <stdio.h>

int safe_pthread_create(void*(*func)(void*), void* arg){
    pthread_t t;
    int ret=pthread_create(&t,NULL,func,arg);
    if(ret){
        fprintf(stderr,"pthread_create failed\n");
        return -1;
    }
    pthread_detach(t);
    return 0;
}
