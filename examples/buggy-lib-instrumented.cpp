#include "../src/shadow-stack.hpp"
#include "do-stuff.h"

int BOOM_OFFSET = 0;

extern "C" void innocent_function(S* s)
{
    s->p += 1;
}

extern "C" void buggy_function(S* s)
{
    s->p += 1;
    shst::invoke(innocent_function, s);

    char a[1];
    a[BOOM_OFFSET] = 0x03; // <-- BOOM

    s->p = a;
}

extern "C" void other(S* s)
{
    s->p += 1;
    shst::invoke(buggy_function, s);
    s->p += 1;
}

extern "C" void some(S* s)
{
    s->p += 1;
    other(s);
    s->p += 1;
}

extern "C" void do_stuff_locked(S* s)
{
    s->p += 1;
    shst::invoke(some, s);
    // free and re-aquire mutex to prevent some compiler optimizations (don't do it at home!)
    pthread_mutex_unlock(&s->m);
    pthread_mutex_lock(&s->m);
    s->p += 1;
}

extern "C" void do_stuff(S* s)
{
    pthread_mutex_lock(&s->m);
    shst::invoke(do_stuff_locked, s);
    pthread_mutex_unlock(&s->m);
}

extern "C" void set_boom_offset(int offset)
{
    BOOM_OFFSET = offset;
}
