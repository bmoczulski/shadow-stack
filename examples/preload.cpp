#include "../src/shadow-stack.h"
#include <map>
#include <dlfcn.h>
#include <stdio.h>

struct RealCallLogger
{
    RealCallLogger(const char *name, shst_f address)
        : name(name)
        , address(address)
    {
        fprintf(stderr, "[shst] %*s--> about to call: %s() = %p\n", depth(), "", name, address);
        depth()++;
    }

    static int& depth()
    {
        static thread_local int depth = 0;
        return depth;
    }

    ~RealCallLogger()
    {
        depth()--;
        fprintf(stderr, "[shst] %*s<-- returned from: %s() = %p\n", depth(), "", name, address);
    }

    const char *name;
    shst_f address;
};

shst_f load_next(const char * name)
{
    static std::map<const char *, shst_f> symbols;
    auto symbol = symbols.find(name);
    if (symbol == symbols.end())
    {
        auto real = reinterpret_cast<shst_f>(dlsym(RTLD_NEXT, name));
        symbols.emplace(name, real);
        return real;
    }
    return symbol->second;
}

#define WRAP(real_function_name) \
extern "C" void* real_function_name(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7) \
{                                                                                                                   \
    auto real = load_next(#real_function_name);                                                                     \
    RealCallLogger logger(#real_function_name, real);                                                               \
    return shst::invoke(real, x0, x1, x2, x3, x4, x5, x6, x7);                                                      \
}

WRAP(do_stuff)
WRAP(do_stuff_locked)
WRAP(some)
WRAP(other)
WRAP(buggy_function)
WRAP(innocent_function)
