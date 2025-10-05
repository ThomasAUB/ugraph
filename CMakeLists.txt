cmake_minimum_required(VERSION 3.13)            # CMake version check
project(ugraph VERSION 0.0.1 LANGUAGES CXX)      # Create project

# Enforce C++17 (library guarantees are written against this version)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

enable_testing()
add_subdirectory(tests)

