#pragma once

namespace shst {

extern "C" long wrapper_impl(void* callee, ...);

namespace details {

template<typename D, typename S>
D mad_cast(S sp)
{
    return *reinterpret_cast<D*>(&sp);
}

} // namespace details

#define a_cast(toType) shst::details::mad_cast<toType>

#define WRAPPER_IMPL(rettype, function, ...) \
    (a_cast(rettype)(shst::wrapper_impl(a_cast(void*)(function), __VA_ARGS__)))

#define WRAPPER_EMPTY(rettype, function) (a_cast(rettype)(shst::wrapper_impl(a_cast(void*)(function))))

#define WRAPPER_VOID(function, ...) (shst::wrapper_impl(a_cast(void*)(function), __VA_ARGS__))

#define WRAPPER_VOID_EMPTY(function) (shst::wrapper_impl(a_cast(void*)(function)))

} // namespace shst
