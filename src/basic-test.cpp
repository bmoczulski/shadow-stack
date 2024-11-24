#include "shadow-stack.h"
#include <cstdio>
#include <functional>
/*#include <algorithm>*/
/*#include <iterator>*/
/*#include <utility>*/

#include <array>

#if 1
int f0([[maybe_unused]] unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    arg_a[12] = 32;
    return 3;
}

int f1(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f0, arg_a, a.data());
}
int f2(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f1, arg_a, a.data());
}

int f3(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f2, arg_a, a.data());
}

int f4(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f3, arg_a, a.data());
}

int f5(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f4, arg_a, a.data());
}

int f6(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f5, arg_a, a.data());
}

int f7(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f6, arg_a, a.data());
}

int f8(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f7, arg_a, a.data());
}

int f9(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f8, arg_a, a.data());
}

int f10(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f9, arg_a, a.data());
}

int f11(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f10, arg_a, a.data());
}

int f12(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f11, arg_a, a.data());
}

int f13(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    /// zapis na stos
    std::array<unsigned, 123> a;
    arg_a[4] = 3;
    shst::invoke(f12, arg_a, a.data());
    return 0;
}

int f14(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f13, arg_a, a.data());
}

int f15(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f14, arg_a, a.data());
}

int f16(unsigned* arg_a, [[maybe_unused]] unsigned* aa = nullptr)
{
    std::array<unsigned, 123> a;
    return shst::invoke(f15, arg_a, a.data());
}
#endif

int foo([[maybe_unused]] void* a, [[maybe_unused]] void* b, [[maybe_unused]] void* c, [[maybe_unused]] void* d)
{
    std::array<unsigned, 8> v;
    return f16(v.data(), nullptr);
}
int bar([[maybe_unused]] int a, [[maybe_unused]] int b, [[maybe_unused]] int c, [[maybe_unused]] int d)
{
    return 0;
}

namespace shst {

struct Foo
{
    bool foo(int a, double b)
    {
        return a == int(b);
    }
};

bool foo(int a, double b)
{
    return a == int(b);
}

void test()
{
    long stack_position;
    detail::guard c(detail::function_address(test), &stack_position);
    Foo f;
    std::function<bool(int, double)> ff{foo};
    ::shst::invoke(foo, 3, 3.14);
    ::shst::invoke(ff, 2, 2.73);
    ::shst::invoke(&Foo::foo, f, 3, 3.14);
}

} // namespace shst

int foo_wrapper(void* a, void* b, void* c, void* d)
{
    return shst::invoke(foo, a, b, c, d);
}

int bar_wrapper(int a, int b, int c, int d)
{
    return shst::invoke(bar, a, b, c, d);
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    bar_wrapper(1, 2, 3, 4);
    foo_wrapper(nullptr, nullptr, nullptr, nullptr);
    shst::test();
}
