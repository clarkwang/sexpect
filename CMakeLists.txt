
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source build not allowed!")
endif()

project(pass C)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_COLOR_MAKEFILE ON)

if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall)
endif()

add_executable(sexpect main.c common.c proto.c pty.c server.c client.c)

find_library(HAVE_LIBRT rt)
if (HAVE_LIBRT)
    target_link_libraries(sexpect rt)
endif()

enable_testing()
add_subdirectory(tests)
