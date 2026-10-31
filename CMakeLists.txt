cmake_minimum_required(VERSION 3.21)
project(wcore LANGUAGES CXX)

add_library(wcore STATIC
    "src/clock.cpp"
    "src/logger.cpp"
    "src/profiler.cpp")
add_library(wcore::wcore ALIAS wcore)

# --- Definitions --- #
target_compile_features(wcore
PUBLIC
    cxx_std_20)
target_compile_definitions(wcore
PUBLIC
    $<$<CONFIG:Debug>:WINK_DEBUG=1>)

# --- FetchContent --- #
include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
# Header-only mode, no library build
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# --- Targets --- #
target_include_directories(wcore
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(wcore
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(wcore PUBLIC glm::glm)
target_link_libraries(wcore PRIVATE spdlog::spdlog)

# --- Tests --- #
option(WCORE_BUILD_TESTS "Build wcore tests" ${PROJECT_IS_TOP_LEVEL})
if(WCORE_BUILD_TESTS)
    enable_testing()
    target_sources(wcore PRIVATE "src/test.cpp")
    add_subdirectory(tests)
endif()
