#pragma once

#include <execinfo.h>
#include <functional>
#include <memory>
#include <string>

namespace callee_traits
{
    // lambda, std::function, function objects
    template<typename T>
    void* address(T)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
        return reinterpret_cast<void*>(&T::operator());
#pragma GCC diagnostic pop
    }

    // free function
    template<typename R, typename... Args>
    void* address(R (*f)(Args...))
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
        return reinterpret_cast<void*>(f);
#pragma GCC diagnostic pop
    }

    // &Klass::Method
    template<typename R, typename C, typename... Args>
    void* address(R (C::*f)(Args...))
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
        return reinterpret_cast<void*>(f);
#pragma GCC diagnostic pop
    }

    // Instance.*(&Klass::Method) - useful for virtual functions
    template<typename R, typename O, typename C, typename... Args>
    void* address(O&& o, R (C::*f)(Args...) const)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
        return reinterpret_cast<void*>(static_cast<const C&>(o).*f);
#pragma GCC diagnostic pop
    }

    // Instance.*(&Klass::Method) - useful for virtual functions
    template<typename R, typename O, typename C, typename... Args>
    void* address(O&& o, R (C::*f)(Args...))
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
        return reinterpret_cast<void*>(static_cast<const C&>(o).*f);
#pragma GCC diagnostic pop
    }

    // (*p).*(&Klass::Method) - useful for virtual functions
    template<typename R, typename O, typename C, typename... Args>
    void* address(O *o, R (C::*f)(Args...))
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
        return reinterpret_cast<void*>((*static_cast<C*>(o)).*f);
#pragma GCC diagnostic pop
    }

    // last resort, whatever pointer, perhaps reinterpred-cased pointer to function
    void* address(void *p)
    {
        return p;
    }


    std::string name(void *callee)
    {
        void *const buffer[] = {callee};
        auto symbols = std::unique_ptr<char*, decltype(free)*>(backtrace_symbols(buffer, 1), free);
        if (symbols) 
        {
            return symbols.get()[0];
        }
        else
        {
            return "<unknown_callee>";
        }
    }

    template<typename... Args>
    static std::string name(Args&& ...args)
    {
        return name(address(std::forward<Args>(args)...));
    }

};
