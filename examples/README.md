# Shadow Stack examples

Wondering how to use Shadow Stack? Here are some handy examples.

## A perfectly legit API

`do-stuff.h` defined an API which is implemented in a library

## A buggy library

The API is implemented in `buggy-lib.c`, which for convenience is build in three flavors:

- static `.a` without instrumentation
- static `.a` with Shadow Stack instrumentation
- shared `.so` without instrumentation, can be instrumented extenrally with LD_PRELOAD

In either flavor the library may corrupt stack at required offset if so instructed by invocation of `set_boom_offset(int)`.

## A poor C executable

An example application using the library and crashing due to the bug. There are two flavors of it:

- Vanilla static - using static library, without any Shadow Stack additions. This one will just crash and there is little that can be done about it.
- Instrumented static - using the static library but with Stadow Stack wrappers in the code. This one will print report when stack corruption is detected.
- Vanilla dynamic - using dynamic library and vanilla code, meaning no Shadow Stack instrumentation here. When executed normally it will crash similarly to vanilla-static version but thanks to the fack that is linked against a shared library it can be instrumented externally with a suitable LD_PRELOAD library.

### pros and cons of each

|                         | Can be instrumented | Code changes necessary | Symbol names in report |
| ----------------------- | ------------------- | ---------------------- | ---------------------- |
| vanilla static          | :no_entry:          | N/A                    | N/A                    |
| instrumented static     | :white_check_mark:  | YES                    | :white_check_mark:     |
| vanilla dynamic         | :white_check_mark:  | NO                     | :x:                    |

## A preload library

`preload.cpp` is a shared library utilising Shadow Stack and suitable for LD_PRELOAD-ing to debug the vanilla-dynamic application.

## Execution

```bash
# without corruption offset set there is no crash
./examples/example-c-static

./examples/example-c-static 100
#                           ^^^  corruption offet
#                                it will differ per system/architecture
#                                you MAY need to adjust it to induce a crash

# another sunny-day scenario
./examples/example-c-dynamic

./examples/example-c-dynamic 200
#                            ^^^  corruption offet

# without rebuilding the app LD_PRELOAD can be used to identify the bug
LD_PRELOAD=./examples/libpreload-lib.so ./examples/example-c-dynamic 200
#                                                   corruption offet ^^^

# instrumented version - no preloading but requires code adjustments and rebuild
./examples example-c-instrumented 100
#                                 ^^^  corruption offet
```
