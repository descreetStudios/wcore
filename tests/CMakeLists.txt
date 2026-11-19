# Dependencies
include(FetchContent)

FetchContent_Declare(Catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_TAG v3.16.1
	GIT_SHALLOW TRUE
	FIND_PACKAGE_ARGS 3 CONFIG
)
FetchContent_MakeAvailable(Catch2)
include(Catch)

# Target
add_executable(wcore_tests
	"log_test.cpp"
	"smoke_test.cpp"
	"types_test.cpp"
	"version_test.cpp"
)

target_compile_definitions(wcore_tests PRIVATE
	WCORE_EXPECTED_VERSION="${PROJECT_VERSION}"
	WCORE_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	WCORE_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	WCORE_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_link_libraries(wcore_tests PRIVATE
	wcore::wcore
	Catch2::Catch2WithMain
)

catch_discover_tests(wcore_tests DISCOVERY_MODE PRE_TEST)