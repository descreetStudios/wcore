cmake_minimum_required(VERSION 3.24)
project(wcore VERSION 0.1.0 LANGUAGES CXX)

# Options
option(WCORE_BUILD_TESTS "Build wcore tests" ${PROJECT_IS_TOP_LEVEL})
option(WCORE_USE_SYSTEM_SPDLOG "Use an installed spdlog instead of fetching it" OFF)

if(PROJECT_IS_TOP_LEVEL)
	set(CMAKE_CXX_EXTENSIONS OFF)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Dependencies
include(cmake/dependencies.cmake)

# Library
add_library(wcore STATIC)
add_library(wcore::wcore ALIAS wcore)

target_sources(wcore PRIVATE
	"src/version.cpp"
	"src/log.cpp"
)

# Version
configure_file(
	cmake/version.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/include/wcore/version.generated.hpp
	@ONLY)

# Target
target_include_directories(wcore PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(wcore PUBLIC spdlog::spdlog)

target_compile_definitions(wcore PUBLIC
	$<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
	$<$<NOT:$<CONFIG:Debug>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>)

target_compile_features(wcore PUBLIC cxx_std_20)

target_compile_options(wcore PRIVATE
	$<$<CXX_COMPILER_ID:MSVC>:/W4>
	$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

set_target_properties(wcore
	PROPERTIES POSITION_INDEPENDENT_CODE ON) # For Linux/macOS shared modules

# Tests
if(WCORE_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
