#include "shadow-stack.h"
#include <cstdint>
#include <iostream>

using namespace std;

bool is_addr_in_range(void* addr, void* from, size_t size)
{
    return (static_cast<uint8_t*>(addr) >= static_cast<uint8_t*>(from)) &&
           (static_cast<uint8_t*>(addr) < (static_cast<uint8_t*>(from) + size));
}

void test_stack_range()
{
    int dummy{};
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    void* stackaddr{};
    size_t stacksize{};
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);
    cout << "Stack: " << stackaddr << ": " << stacksize << ": local var: " << &dummy
         << " in range: " << is_addr_in_range(&dummy, stackaddr, stacksize) << '\n';
}

//////////////////////////////////////////////////////////////////////////
/// experiment
//////////////////////////////////////////////////////////////////////////

long foo(...)
{
    return 0;
}

long foo_wrapper(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
{
    return shst::wrapper_impl(reinterpret_cast<void*>(foo), x0, x1, x2, x3, x4, x5, x6, x7);
}

//////////////////////////////////////////////////////////////////////////
/// end of experiment
//////////////////////////////////////////////////////////////////////////


int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    std::cout << "main\n";
    std::cout << "main done\n";
}
