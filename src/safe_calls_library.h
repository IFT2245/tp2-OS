#ifndef SAFE_CALLS_LIBRARY_H
#define SAFE_CALLS_LIBRARY_H

int safe_pthread_create(void*(*func)(void*), void* arg);

#endif
