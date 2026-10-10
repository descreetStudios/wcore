cmake_minimum_required(VERSION 3.21)
project(wcore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wcore)

# --- Definitions --- #
target_compile_definitions(wcore
PUBLIC
    $<$<CONFIG:debug>:WINK_DEBUG=1>)

# --- Targets --- #
target_include_directories(wcore
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(wcore
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(wcore
PRIVATE
    "src/test.cpp")

# --- Tests --- #
option(WCORE_BUILD_TESTS "Build wcore tests" ON)
if(WCORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
