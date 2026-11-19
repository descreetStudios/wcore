include(FetchContent)

# spdlog
if(WCORE_USE_SYSTEM_SPDLOG)
	find_package(spdlog CONFIG REQUIRED)
elseif(NOT TARGET spdlog::spdlog)
	FetchContent_Declare(spdlog
		GIT_REPOSITORY https://github.com/gabime/spdlog.git
		GIT_TAG v1.17.0
		GIT_SHALLOW TRUE)
	FetchContent_MakeAvailable(spdlog)
endif()