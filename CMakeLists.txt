cmake_minimum_required(VERSION 3.16.0)

# Config Version
set(AMUN_VERSION_MAJOR 0)
set(AMUN_VERSION_MINOR 0)
set(AMUN_VERSION_PATCH 1)
set(AMUN_VERSION "${AMUN_VERSION_MAJOR}.${AMUN_VERSION_MINOR}.${AMUN_VERSION_PATCH}")

project(amun VERSION ${AMUN_VERSION})

# Config C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# C++ Compiler options
add_compile_options(-Wall)
add_compile_options(-Wextra)
add_compile_options(-pedantic)
add_compile_options(-Wno-unused-parameter)
add_compile_options(-Wno-uninitialized)
add_compile_options(-Wno-missing-field-initializers)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin CACHE INTERNAL "")

# Use ccache if possible
FIND_PROGRAM(CCACHE_PROGAMUNM ccache)
IF(CCACHE_PROGAMUNM)
    MESSAGE(STATUS "Found ccache ${CCACHE_PROGAMUNM}")
ENDIF()

# Config LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Check if LLVM Version is valid
if (${LLVM_PACKAGE_VERSION} VERSION_LESS "14.0")
  message(FATAL_ERROR "LLVM version is too low, need at last v14.0")
endif()

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

file(GLOB source_files CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/include/*.hpp"
)

add_executable(amun ${source_files})

# Map Cpmponent to libAMUNries names
llvm_map_components_to_libnames(llvm_libs
    Core
    MC
    Support
    native
)

target_link_libraries(amun ${llvm_libs})