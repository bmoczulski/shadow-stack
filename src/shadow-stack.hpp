#pragma once

#include "callee_traits.hpp"
#include <type_traits>
#include <functional>

namespace shst {
namespace detail {

struct guard
{
    guard(void* callee, void* stack_pointer);
    ~guard();
};

} // namespace detail

template <class F, class... Args>
constexpr auto invoke(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
{
    long stack_position;
    detail::guard c(callee_traits::address(std::forward<F>(f), std::forward<Args>(args)...), &stack_position);
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace shst

