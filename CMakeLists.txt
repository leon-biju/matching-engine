cmake_minimum_required(VERSION 3.25)
project(matching_engine LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(core)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
