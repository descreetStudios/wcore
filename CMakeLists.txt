cmake_minimum_required(VERSION 3.24)
project(wcore VERSION 0.1.0 LANGUAGES CXX)

# Options
option(WCORE_BUILD_TESTS "Build wcore tests" ${PROJECT_IS_TOP_LEVEL})
option(WCORE_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})

if(PROJECT_IS_TOP_LEVEL)
	set(CMAKE_CXX_EXTENSIONS OFF)
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Library
add_library(wcore STATIC)
add_library(wcore::wcore ALIAS wcore)

target_sources(wcore PRIVATE
	"src/version.cpp"
)

# Version
configure_file(
	cmake/version.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/include/wcore/version.generated.hpp
	@ONLY)

# Target
target_include_directories(wcore PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

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

# Install
if(WCORE_INSTALL)
	include(GNUInstallDirs)
	include(CMakePackageConfigHelpers)

	install(TARGETS wcore EXPORT wcoreTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
	install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/wcore/version.generated.hpp
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wcore)

	install(EXPORT wcoreTargets 
		NAMESPACE wcore:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wcore)

	configure_package_config_file(
		cmake/wcoreConfig.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/wcoreConfig.cmake
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wcore)
	write_basic_package_version_file(
		${CMAKE_CURRENT_BINARY_DIR}/wcoreConfigVersion.cmake
		COMPATIBILITY SameMajorVersion)

	install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/wcoreConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/wcoreConfigVersion.cmake
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wcore)
endif()