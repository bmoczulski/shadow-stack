#include "shadow-stack.h"
#include <cstdio>
#include <array>

int f0([[maybe_unused]] unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    return 3;
}

int f1(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f0, arg_a, a);
}

int f2(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f1, arg_a, a);
}

int f3(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f2, arg_a, a);
}

int f4(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f3, arg_a, a);
}

int f5(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f4, arg_a, a);
}

int f6(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f5, arg_a, a);
}

int f7(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f6, arg_a, a);
}

int f8(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f7, arg_a, a);
}

int f9(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f8, arg_a, a);
}

int f10(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f9, arg_a, a);
}

int f11(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f10, arg_a, a);
}

int f12(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f11, arg_a, a);
}

int f13(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    /// zapis na stos
    std::array<unsigned, 123> a;
    arg_a[4] = 3;
    WRAPPER_IMPL(int, f12, arg_a, a);
    return 0;
}

int f14(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f13, arg_a, a);
}

int f15(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f14, arg_a, a);
}

int f16(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return WRAPPER_IMPL(int, f15, arg_a, a);
}

int foo(void* a, void* b, void* c, void* d)
{
    printf("%p %p %p %p\n", a, b, c, d);
    std::array<unsigned, 8> v;
    return f16(v.data(), nullptr);
}
int bar(int a, int b, int c, int d)
{
    printf("%d %d %d %d\n", a, b, c, d);
    return 0;
}

int foo_wrapper(void* a, void* b, void* c, void* d)
{
    return WRAPPER_IMPL(int, foo, a, b, c, d);
}

int bar_wrapper(int a, int b, int c, int d)
{
    return WRAPPER_IMPL(int, bar, a, b, c, d);
}


int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    bar_wrapper(1, 2, 3, 4);
}

