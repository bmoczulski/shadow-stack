# Shadow Stack

A library for finding stack corruptions.

## Slides

Slides from code::dive 2024 presentation are available here:

[![Shadow Stakc - slides (PDF)](https://bartosz.codes/assets/Shadow%20Stack,%20code__dive%202024%20-%20title%20slide.png)](https://bartosz.codes/assets/Shadow%20Stack,%20code__dive%202024.pdf)

# Why?

Because deep sub-sub-callee can corrupt (grand-)parent stack frame in a manner diffucult to debug and only manifesting itself way later!

Imagine the following C function:

```C
void do_stuff(struct S *s) {
    pthread_mutex_lock(&s->mutex);
    do_stuff_locked(s);
    pthread_mutex_unlock(&s->mutex);
}
```

and the following call sequence:

```
do_stuff()
    pthread_mutex_lock()
    do_stuff_locked()
        some()
            other()
                buggy_function()         # <-- ERROR HERE
                # here, buggy_function() corrupts stack frame of
                # do_stuff_locked() which will only cause crash
                # in pthread_mutex_unlock() back in do_stuff()
    pthread_mutex_unlock()               # <-- BOOM HERE! :(
```

Finding such errors with just GDB may be challenging, regardless if it is post-mortem coredump analysis of live execution.

Shadow Stack aims to help with it.

# How?

By adding interception points at pre-defined code locations and checking consistency of the entire stack above pre-call and post-return.

```
do_stuff()
    shadow_stack_check_call()
    do_stuff_locked()
        shadow_stack_check_call()
        some()
            shadow_stack_check_call()
            other()
                shadow_stack_check_call()
                buggy_function()
                shadow_stack_check_return()  # <-- error detected already HERE
            shadow_stack_check_return()
        shadow_stack_check_return()
    shadow_stack_check_return()
    pthread_mutex_unlock()                   # <-- no need to wait until here
```

# How to use?

There are two ways - code change of LD_PRELOAD (see [examples](examples/README.md) for details).

## Change your code

In C function:

```C
void do_stuff(S *s) {
    // ...
    do_stuff_locked(s);              // don't do this
    shst_invoke(do_stuff_locked, s); // do this instead
    // ...
}
```

Works with C++ too:

```C++
void do_stuff(S *s) {
    // ...
    do_stuff_locked(s);               // don't do this
    shst::invoke(do_stuff_locked, s); // do this instead
    // ...
}
```

Method calls can be a bit trickier but still supported:

```C++
void do_stuff(S *s) {
    // ...
    s->brew(0xC0FFEE);                   // don't do this
    shst::invoke(&S::brew, s, 0xC0FFEE); // do this instead
    // ...
}
```

## LD_PRELOAD

Prealoadble library can be implemented either in C and C++, whatever is more convenient.

```C++
// WARNING! Shameless ABI abuse follows, dragons ahead!
using shst_f = void* (*)(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7);

// wrap C or `extern "C"` function like that
extern "C" void* do_stuff(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
{
  auto real = reinterpret_cast<shst_f>(dlsym(RTLD_NEXT, "do_stuff"));
  return shst::invoke(real, x0, x1, x2, x3, x4, x5, x6, x7);
}

// works with mangled C++ names too, of course
extern "C" void* _Z3fooP1S(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
{
  auto real = reinterpret_cast<shst_f>(dlsym(RTLD_NEXT, "_Z3fooP1S"));
  return shst::invoke(real, x0, x1, x2, x3, x4, x5, x6, x7);
}
```

# Building

Usual CMake flow, e.g. like that:

```
cmake -S . -B build
make -C build
```

I personally prefer Ninja :ninja:

```
cmake -S . -B build -G Ninja
ninja -C build
```

# Environment variables

`SHST_REACTION` - what should Shadow Stack do when it detects a corruption

- `"ignore"` - see no evil, don't report anything, continue execution
- `"report"` - print report, continue execution
- `"abort"` (default action) - print report and call `abort()`
- `"heal"` - print report, restore correct stack from shadow copy, continue execution
- `"quiet-heal"` - restore correct stack from shadow copy, continue execution without printing any report

`SHST_DUMP_WIDTH` - how wide the hex-dump should be (bytest per line)

- this should be an integer
- default is `16`

`SHST_DUMP_AREA` - which area should be shown in hex-dump

- `"both"` (default) - show both actual stack and shadow copy
- `"actual"` - show only actual stack (i.e. the corrupted stack)
- `"shadow"` - show only shadow copy (i.e. the correct stack)

`SHST_DUMP_HIDE_EQUAL` - should equal lines in hex-dump be hidden

- `"yes|true|1"` - hide equal lines (show only differing lines)
- anything else (default) - show everything
