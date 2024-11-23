#pragma once
#include <pthread.h>

struct S
{
    char *p;
    pthread_mutex_t m;
};

#ifndef __cplusplus
typedef struct S S;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void do_stuff(S *s);
void set_boom_offset(int offset);

#ifdef __cplusplus
} // extern "C"
#endif
