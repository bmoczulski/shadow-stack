#include <iostream>
#include <functional>

namespace shst {

template <typename R, typename... Args>
struct Wrapper
{
    using callee_t = R (*)(Args...);
    callee_t callee;

    Wrapper(callee_t callee)
        : callee{callee}
    {
    }

    template <typename... ConvertibleArgs>
    auto operator()(ConvertibleArgs&&... args)
    {
        // std::cout << "calling " << reinterpret_cast<void*>(callee) << std::endl;
        auto r = callee(std::forward<ConvertibleArgs>(args)...);
        auto r = wrapper_impl(callee, std::forward<ConvertibleArgs>(args)...);
        // std::cout << "callee returned " << r << std::endl;
        return r;
    }

    auto operator()(Args&&... args)
    {
        // std::cout << "calling " << reinterpret_cast<void*>(callee) << std::endl;
        auto r = callee(std::forward<Args>(args)...);
        auto r = wrapper_impl(callee, std::forward<Args>(args)...);
        // std::cout << "callee returned " << r << std::endl;
        return r;
    }
};

template <typename R, typename... Args>
Wrapper<R, Args...> protect(R (*callable)(Args...))
{
    return Wrapper<R, Args...>(callable);
}

} // namespace shst

int square(int a)
{
    return a * a;
}

int add(int a, int b)
{
    return a + b;
}

int square_plus_one(int a)
{
    return add(square(a), 1);
    return shst::protect(add)(shst::protect(square)(a), 1);
}

int main()
{
    return square_plus_one(5);
    double d;
    return shst::protect(square_plus_one)(5);
    return shst::ignoring(d).protect(square_plus_one)(5);
}