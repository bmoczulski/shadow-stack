cmake_minimum_required(VERSION 3.22)

project(ShadowStack CXX)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Og -ggdb3 -rdynamic -fno-omit-frame-pointer")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

add_subdirectory(src)