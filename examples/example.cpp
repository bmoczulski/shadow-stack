#include "do-stuff.h"
#include <stdlib.h>

S s;

int main(int argc, char** argv)
{
    if (argc > 1) {
        set_boom_offset(atoi(argv[1]));
    }
    pthread_mutex_init(&s.m, nullptr);
    do_stuff(&s);
    pthread_mutex_destroy(&s.m);
}
