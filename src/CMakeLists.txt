
set(SHST_LIBRARY_SOURCES shadow-stack.h shadow-stack.cpp)

add_library(shst ${SHST_LIBRARY_SOURCES})

add_executable(basic-test basic-test.cpp)
target_link_libraries(basic-test shst)

add_executable(main-test main.cpp)
target_link_libraries(main-test shst)

