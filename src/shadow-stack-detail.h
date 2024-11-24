#pragma once

typedef void* (*shst_f)(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7);

#ifdef __cplusplus

#include <type_traits>
#include <functional>

namespace shst {
namespace detail {

struct guard
{
    guard(void* callee, void* stack_pointer);
    ~guard();
};

template <typename F>
void* function_address(F&& f)
{
    /// TODO: get function address (if possible) from
    /// standalone function,
    /// member function
    /// from lambda
    /// std::function
    return reinterpret_cast<void*>(&f);
}

} // namespace detail

void ignore_above(void* stack_pointer);

template <class F, class... Args>
constexpr auto invoke(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
{
    long stack_position;
    detail::guard c(detail::function_address(f), &stack_position);
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace shst

#endif // __cplusplus
