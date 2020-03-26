cmake_minimum_required(VERSION 2.8)

project(uloop_unit_test C CXX)

set(CMAKE_BUILD_TYPE DEBUG)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set (PROFILER_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/prof)

find_package(PkgConfig REQUIRED)
pkg_search_module(CPPUTEST REQUIRED cpputest>=3.8)
message(STATUS "CppUTest version ${CPPUTEST_VERSION}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=gnu++11 -O0 -fprofile-arcs -ftest-coverage -DDEBUG -Wno-c++14-compat")

add_subdirectory(uloop)
add_subdirectory(uloop_timer)