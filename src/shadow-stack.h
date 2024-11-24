#pragma once

#include "shadow-stack-common.h"

#ifdef __cplusplus
#define MAYBE_EXTERN_C extern "C"
#else
#define MAYBE_EXTERN_C
#endif

MAYBE_EXTERN_C
void* shst_invoke_impl(void* callee, ...);


