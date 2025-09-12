# C
add_library(buggy-lib-shared SHARED buggy-lib.c)
add_library(buggy-lib-static buggy-lib.c)
add_library(buggy-lib-instrumented buggy-lib-instrumented.c)
target_link_libraries(buggy-lib-instrumented PRIVATE shst-static)

add_executable(example-c-static example.c)
target_link_libraries(example-c-static buggy-lib-static)

add_executable(example-c-instrumented example-instrumented.c)
target_link_libraries(example-c-instrumented PRIVATE buggy-lib-instrumented shst-static)

add_executable(example-c-dynamic example.c)
target_link_libraries(example-c-dynamic buggy-lib-shared)

add_library(preload-lib SHARED preload.cpp)
target_link_libraries(preload-lib dl shst)


# C++
add_library(buggy-lib++-shared SHARED buggy-lib.cpp)
add_library(buggy-lib++-static buggy-lib.cpp)
add_library(buggy-lib++-instrumented buggy-lib-instrumented.cpp)
target_link_libraries(buggy-lib++-instrumented PRIVATE shst-static)

add_executable(example-c++-static example.cpp)
target_link_libraries(example-c++-static buggy-lib++-static)

add_executable(example-c++-instrumented example-instrumented.cpp)
target_link_libraries(example-c++-instrumented PRIVATE buggy-lib++-instrumented shst-static)

add_executable(example-c++-dynamic example.cpp)
target_link_libraries(example-c++-dynamic buggy-lib++-shared)


add_executable(example-c++-object do-stuff.cpp)
target_link_libraries(example-c++-object shst-static)


# execinfo

find_path(LIBEXECINFO_INCLUDE_DIR execinfo.h)
find_library(LIBEXECINFO_LIBRARY NAMES execinfo)

if(LIBEXECINFO_INCLUDE_DIR AND LIBEXECINFO_LIBRARY)
  set(LIBEXECINFO_FOUND TRUE)
endif()

# libunwind

find_path(LIBUNWIND_INCLUDE_DIR libunwind.h)
find_library(LIBUNWIND_LIBRARY NAMES unwind)

if(LIBUNWIND_INCLUDE_DIR AND LIBUNWIND_LIBRARY)
  set(LIBUNWIND_FOUND TRUE)
endif()


# Backtrace test
add_executable(backtrace-test backtrace-test.c)
# Add C flags similar to C++ flags for proper unwinding
set_target_properties(backtrace-test PROPERTIES 
    COMPILE_FLAGS "-Wall -Wextra -Og -ggdb3 -rdynamic -fno-omit-frame-pointer -funwind-tables -fasynchronous-unwind-tables")
if (LIBEXECINFO_FOUND)
    # execinfo must be linked explicitly for musl libc (e.g.: in alpine qemu)
    target_link_libraries(backtrace-test execinfo)
endif ()

# Alternative backtrace test with minimal flags (for comparison)
add_executable(backtrace-test-minimal backtrace-test.c)
set_target_properties(backtrace-test-minimal PROPERTIES 
    COMPILE_FLAGS "-g -fno-omit-frame-pointer")
if (LIBEXECINFO_FOUND)
    target_link_libraries(backtrace-test-minimal execinfo)
endif ()

# Libunwind-enabled backtrace test
if (LIBUNWIND_FOUND)
add_executable(backtrace-test-libunwind backtrace-test.c)
set_target_properties(backtrace-test-libunwind PROPERTIES 
    COMPILE_FLAGS "-Wall -Wextra -Og -ggdb3 -rdynamic -fno-omit-frame-pointer -DHAVE_LIBUNWIND")
if (LIBEXECINFO_FOUND)
    target_link_libraries(backtrace-test-libunwind execinfo)
endif ()
    target_link_libraries(backtrace-test-libunwind unwind)
endif ()
