cmake_minimum_required(VERSION 3.20)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Preventing in-source building. Please run cmake in a separate build directory.")
endif ()

project(
        TP2
        VERSION 24
        DESCRIPTION "Scheduling"
        LANGUAGES C)

set(CMAKE_C_STANDARD 17)
include(cmake/dependencies.cmake)
add_subdirectory(src)