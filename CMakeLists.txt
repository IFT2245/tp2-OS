cmake_minimum_required(VERSION 3.20)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory.")
endif()

project(
TP2
VERSION 24
DESCRIPTION "Real-Time Process Scheduler"
LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
include(cmake/dependencies.cmake)
# Option to enable OpenGL-based UI
option(USE_OPENGL_UI "Enable OpenGL-based UI code" ON)

add_subdirectory(src)