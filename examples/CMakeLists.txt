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
