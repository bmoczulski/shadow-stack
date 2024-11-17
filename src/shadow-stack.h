#pragma once

namespace shst {

extern "C" long invoke_impl(void* callee, ...);

namespace details {



} // namespace details

#define a_cast(toType) shst::details::mad_cast<toType>

#define WRAPPER_IMPL(rettype, function, ...) \
    (a_cast(rettype)(shst::invoke_impl(a_cast(void*)(function), __VA_ARGS__)))

#define WRAPPER_EMPTY(rettype, function) (a_cast(rettype)(shst::invoke_impl(a_cast(void*)(function))))

#define WRAPPER_VOID(function, ...) (shst::invoke_impl(a_cast(void*)(function), __VA_ARGS__))

#define WRAPPER_VOID_EMPTY(function) (shst::invoke_impl(a_cast(void*)(function)))

} // namespace shst
