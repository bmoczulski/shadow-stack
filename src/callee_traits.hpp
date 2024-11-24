#pragma once

#include <execinfo.h>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace callee_traits {

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
using remove_reference_and_pointer_t = std::remove_pointer_t<std::remove_reference_t<T>>;

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

template <typename T>
void* to_void_pointer(T&& o)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(o);
#pragma GCC diagnostic pop
}

// lambda, std::function, function objects
template <typename T>
void* address(T&&)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    static_assert(std::is_object_v<T>);
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
template <typename R, typename C, typename... Args>
void* address(R (C::*f)(Args...))
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(f);
#pragma GCC diagnostic pop
}

template <typename R, typename C, typename... Args>
void* address(R (C::*f)(Args...) const)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(f);
#pragma GCC diagnostic pop
}

// Instance.*(&Klass::Method) - useful for virtual functions
template <typename R, typename O, typename C, typename... Args>
void* address(O&& o, R (C::*f)(Args...) const)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(
            (static_cast<copy_cv_t<remove_reference_and_pointer_t<O>, C>*>(object_pointer(o)))->*f);
#pragma GCC diagnostic pop
}

template <typename R, typename O, typename C, typename... Args>
void* address(O&& o, R (C::*f)(Args...))
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    return reinterpret_cast<void*>(
            (static_cast<copy_cv_t<remove_reference_and_pointer_t<O>, C>*>(object_pointer(o)))->*f);
#pragma GCC diagnostic pop
}

// last resort, whatever pointer, perhaps reinterpred-cased pointer to function
void* address(void* p)
{
    return p;
}

std::string name(void* callee)
{
    void* buffer[]{const_cast<void*>(callee)};
    auto symbols = std::unique_ptr<char*, decltype(free)*>(backtrace_symbols(buffer, 1), free);
    if (symbols) {
        if (symbols.get()[0]) {
            return symbols.get()[0];
        } else {
            std::ostringstream os;
            os << std::hex << std::showbase << callee;
            return os.str();
        }
    } else {
        return "<unknown_callee>";
    }
}

template <typename... Args>
static std::string name(Args&&... args)
{
    return name(address(std::forward<Args>(args)...));
}

};// namespace callee_traits
