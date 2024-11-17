#include "shadow-stack.h"
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <utility>

/*#include <array>*/

#if 0
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
#endif

int foo([[maybe_unused]] void* a, [[maybe_unused]] void* b, [[maybe_unused]] void* c, [[maybe_unused]] void* d)
{
    /*std::array<unsigned, 8> v;*/
    /*f16(v.data(), nullptr);*/
    return 0;
}
int bar([[maybe_unused]] int a, [[maybe_unused]] int b, [[maybe_unused]] int c, [[maybe_unused]] int d)
{
    return 0;
}

namespace shst {
namespace detail {

template <class>
constexpr bool is_reference_wrapper_v = false;
template <class U>
constexpr bool is_reference_wrapper_v<std::reference_wrapper<U>> = true;

template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <auto m>
struct StaticFunctionFactory;

/*template <typename R, typename C, typename... A, R (C::*m)(A...)>*/
/*struct StaticFunctionFactory<m>*/
/*{*/
/*    template <size_t... Idxs>*/
/*    static auto make_static_function_impl(std::index_sequence<Idxs...>)*/
/*    {*/
/*        struct Impl*/
/*        {*/
/*            static R call(Object&& p, [[maybe_unused]] std::tuple_element_t<Idxs, std::tuple<A...>>... args)*/
/*            {*/
/*                return p->*m(std::forward<std::tuple_element_t<Idxs, std::tuple<A...>>>(args)...);*/
/*            }*/
/*        };*/
/**/
/*        return &Impl::call;*/
/*    }*/
/**/
/*    static auto make_static_function()*/
/*    {*/
/*        return make_static_function_impl(std::index_sequence<sizeof...(A)>{});*/
/*    }*/
/*};*/

template <typename R, typename... A, R (*m)(A...)>
struct StaticFunctionFactory<m>
{

    template <size_t... Idxs>
    static auto make_static_function_impl(std::index_sequence<Idxs...>)
    {
        struct Impl
        {
            static R call([[maybe_unused]] std::tuple_element_t<Idxs, std::tuple<A...>>... args)
            {
                return m(std::forward<std::tuple_element_t<Idxs, std::tuple<A...>>>(args)...);
            }
        };

        return &Impl::call;
    }

    static auto make_static_function()
    {
        return make_static_function_impl(std::index_sequence<sizeof...(A)>{});
    }
};

bool foo(int a, double b)
{
    return a == int(b);
}

template <class F, class... Args>
constexpr std::invoke_result_t<F, Args...> foo_invoke(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
{
    /*return invoke_impl(StaticFunctionFactory<f>::make_static_function(), std::forward<Args>(args)...);*/
    return false;
}
struct Foo {
    bool foo(int a, double b)
    {
        return a == int(b);
    }
};
void test() {
    foo_invoke(foo, 3, 3.14);
    Foo f;
    foo_invoke(&Foo::foo, 3, 3.14);
}

template <class C, class Pointed, class Object, class... Args>
constexpr decltype(auto) invoke_memptr(Pointed C::* member, Object&& object, Args&&... args)
{
    using object_t = remove_cvref_t<Object>;
    constexpr bool is_member_function = std::is_function_v<Pointed>;
    constexpr bool is_wrapped = is_reference_wrapper_v<object_t>;
    constexpr bool is_derived_object = std::is_same_v<C, object_t> || std::is_base_of_v<C, object_t>;

    if constexpr (is_member_function) {
        if constexpr (is_derived_object)
            return (std::forward<Object>(object).*member)(std::forward<Args>(args)...);
        else if constexpr (is_wrapped)
            return (object.get().*member)(std::forward<Args>(args)...);
        else
            return ((*std::forward<Object>(object)).*member)(std::forward<Args>(args)...);
    } else {
        static_assert(std::is_object_v<Pointed> && sizeof...(args) == 0);
        if constexpr (is_derived_object)
            return std::forward<Object>(object).*member;
        else if constexpr (is_wrapped)
            return object.get().*member;
        else
            return (*std::forward<Object>(object)).*member;
    }
}
}// namespace detail

template <class F, class... Args>
constexpr std::invoke_result_t<F, Args...> invoke(F&& f,
                                                  Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
{
    if constexpr (std::is_member_pointer_v<detail::remove_cvref_t<F>>)
        return detail::invoke_memptr(f, std::forward<Args>(args)...);
    else {
        return std::forward<F>(f)(std::forward<Args>(args)...);
    }
}
}// namespace shst

/*int foo_wrapper(void* a, void* b, void* c, void* d)*/
/*{*/
/*    return WRAPPER_IMPL(int, foo, a, b, c, d);*/
/*}*/
/**/
/*int bar_wrapper(int a, int b, int c, int d)*/
/*{*/
/*    return WRAPPER_IMPL(int, bar, a, b, c, d);*/
/*}*/

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    /*bar_wrapper(1, 2, 3, 4);*/
}
