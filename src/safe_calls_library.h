#ifndef SAFE_CALLS_LIBRARY_H
#define SAFE_CALLS_LIBRARY_H

int safe_pthread_create(void*(*f)(void*),void* arg);
int parse_int_strtol(const char* input,int fallback);
long parse_long_strtol(const char* input,long fallback);
float parse_float_strtof(const char* input,float fallback);
double parse_double_strtod(const char* input,double fallback);

#endif
