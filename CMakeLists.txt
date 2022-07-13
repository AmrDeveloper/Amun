cmake_minimum_required(VERSION 3.0.0)

# Config Version
set(JOT_VERSION_MAJOR 0)
set(JOT_VERSION_MINOR 0)
set(JOT_VERSION_PATCH 1)
set(JOT_VERSION "${JOT_VERSION_MAJOR}.${JOT_VERSION_MINOR}.${JOT_VERSION_PATCH}")

project(jot VERSION ${JOT_VERSION})

# Config C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic -Wuninitialized")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Use ccache if possible
FIND_PROGRAM(CCACHE_PROGRAM ccache)
IF(CCACHE_PROGRAM)
    MESSAGE(STATUS "Found ccache ${CCACHE_PROGRAM}")
ENDIF()

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

file(GLOB source_files CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/include/*.h"
)

add_executable(jot ${source_files})