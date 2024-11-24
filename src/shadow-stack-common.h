#pragma once


#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*shst_f)(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7);
#define shst_invoke(f, ...) (typeof(f(__VA_ARGS__)))shst_invoke_impl((shst_f)f, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
