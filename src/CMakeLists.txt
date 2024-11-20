find_path(LIBEXECINFO_INCLUDE_DIR execinfo.h)
find_library(LIBEXECINFO_LIBRARY NAMES execinfo)

if(LIBEXECINFO_INCLUDE_DIR AND LIBEXECINFO_LIBRARY)
  set(LIBEXECINFO_FOUND TRUE)
endif()

set(SHST_LIBRARY_SOURCES shadow-stack.h shadow-stack.cpp)

add_library(shst ${SHST_LIBRARY_SOURCES})
if (LIBEXECINFO_FOUND)
    # execinfo must be linked explicitly for musl libc (e.g.: in alpine qemu)
    target_link_libraries(shst execinfo)
endif ()

add_executable(basic-test basic-test.cpp)
target_link_libraries(basic-test shst)

add_executable(main-test main.cpp)
target_link_libraries(main-test shst)

add_executable(callee_traits-test callee_traits-test.cpp)
