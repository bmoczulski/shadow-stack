#pragma once

#include <execinfo.h>
#include <string>
#include <type_traits>
#include <utility>

namespace callee_traits {
namespace detail {
template <typename From, typename To>
struct copy_const
{
    using type = std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, std::remove_const_t<To>>;
};

template <typename From, typename To>
using copy_const_t = typename copy_const<From, To>::type;

template <typename From, typename To>
struct copy_volatile
{
    using type = std::conditional_t<std::is_volatile_v<From>, std::add_volatile_t<To>, std::remove_volatile_t<To>>;
};

template <typename From, typename To>
using copy_volatile_t = typename copy_volatile<From, To>::type;

template <typename From, typename To>
struct copy_cv
{
    using type = copy_const_t<From, copy_volatile_t<From, To>>;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

template <typename T>
struct remove_pr
{
	using type = T;
};
template <typename T>
struct remove_pr<T*>
{
    using type = std::remove_pointer_t<T>;
};

template <typename T>
struct remove_pr<T&>
{
    using type = std::remove_reference_t<T>;
};

template <typename T>
using remove_reference_and_pointer_t = typename remove_pr<T>::type;

template <typename O>
O* object_pointer(O& oref)
{
    return &oref;
}

template <typename O>
O* object_pointer(O* optr)
{
    return optr;
}
// lambda, std::function, function objects
template <typename T>
void* address(T&&)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(&remove_reference_and_pointer_t<T>::operator());
#pragma GCC diagnostic pop
}

// free function
template <typename R, typename... Args>
void* address(R (*f)(Args...))
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(f);
#pragma GCC diagnostic pop
}

// &Klass::Method
template <typename R, typename C, typename... Args, typename... Tail>
void* member_function_address(R (C::*f)(Args...), Tail&&...)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(f);
#pragma GCC diagnostic pop
}

template <typename R, typename C, typename... Args, typename... Tail>
void* member_function_address(R (C::*f)(Args...) const, Tail&&...)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(f);
#pragma GCC diagnostic pop
}

// Instance.*(&Klass::Method) - useful for virtual functions
template <typename R, typename O, typename C, typename... Args, typename... Tail>
void* member_function_address(R (C::*f)(Args...) const, O&& o, Tail&&...)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(
            (static_cast<copy_cv_t<remove_reference_and_pointer_t<O>, C>*>(object_pointer(o)))->*f);
#pragma GCC diagnostic pop
}

template <typename R, typename O, typename C, typename... Args, typename... Tail>
void* member_function_address(R (C::*f)(Args...), O&& o, Tail&&...)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(
            (static_cast<copy_cv_t<remove_reference_and_pointer_t<O>, C>*>(object_pointer(o)))->*f);
#pragma GCC diagnostic pop
}

// last resort, whatever pointer, perhaps reinterpred-cased pointer to function
void* address(void* p);

} // namespace detail



template <typename F, typename... Args>
void* address(F&& f, Args&&... args)
{
    if constexpr (std::is_member_function_pointer_v<detail::remove_reference_and_pointer_t<F>>) {
        return detail::member_function_address(std::forward<F>(f), std::forward<Args>(args)...);
    } else {
        if constexpr (std::is_function_v<detail::remove_reference_and_pointer_t<F>>) {
            return reinterpret_cast<void*>(f);
        }
        if constexpr (std::is_object_v<detail::remove_reference_and_pointer_t<F>>) {
            return detail::address(std::forward<F>(f));
        }
    }
    return nullptr;
}

std::string name(void* callee);

template <typename... Args>
std::string name(Args&&... args)
{
    return name(address(std::forward<Args>(args)...));
}

}; // namespace callee_traits
