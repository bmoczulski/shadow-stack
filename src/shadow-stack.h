#pragma once
#include "shadow-stack-detail.h"

#define shst_invoke(f, ...) (typeof(f(__VA_ARGS__)))shst_invoke_impl((shst_f)f, ##__VA_ARGS__)

#ifdef __cplusplus
#define MAYBE_EXTERN_C extern "C"
#else
#define MAYBE_EXTERN_C
#endif

MAYBE_EXTERN_C
void* shst_invoke_impl(void* callee, ...);
