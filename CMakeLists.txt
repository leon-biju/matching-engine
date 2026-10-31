cmake_minimum_required(VERSION 3.25)
project(matching_engine LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Shared header-only types used by both engines
add_library(matching_core INTERFACE)
add_library(matching::core ALIAS matching_core)
target_include_directories(matching_core INTERFACE include)
target_compile_features(matching_core INTERFACE cxx_std_23)

add_subdirectory(src/version1)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(benchmarks)
