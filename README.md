<div align="center">

# wcore

**A C++20 foundation for modern projects.**

<a href="https://isocpp.org/"><img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20"></a>
<a href="https://cmake.org/"><img src="https://img.shields.io/badge/CMake-3.24%2B-064F8C?logo=cmake&logoColor=white" alt="CMake 3.24+"></a>

</div>

`wcore` is a shared core library for C++ projects. It provides a small, stable foundation that grows as the projects using it grow: a static library, a single CMake target (`wcore::wcore`), and headers under `<wcore/...>`.

The library is deliberately generic. Today it contains a couple of small modules; more will be added over time without changing how you build, link, or include it.

> **Status:** early and evolving. New modules may be added, but the package name, target name, and header layout are intended to remain stable.

## Requirements

- CMake 3.24 or newer
- A compiler with C++20 support
- Ninja, for the `linux-ninja` preset
- Network access on the first configure (Catch2 and spdlog are fetched by CMake)

## Get the source

```sh
git clone https://github.com/descreetStudios/wcore.git
cd wcore
```

## Build

The quickest path is one of the bundled presets:

```sh
# Windows (Visual Studio)
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug

# Linux (Ninja Multi-Config)
cmake --preset linux-ninja
cmake --build build/linux-ninja --config Debug
```

Or configure manually:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Build options

| Option | Default | Description |
| --- | --- | --- |
| `WCORE_BUILD_TESTS` | `ON` when top-level | Build the test suite |
| `WCORE_USE_SYSTEM_SPDLOG` | `OFF` | Use an installed spdlog instead of fetching it |

When `wcore` is added as a subdirectory of another project, tests default to `OFF`, so the parent build is not affected.

## Run the tests

```sh
ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

The tests use [Catch2](https://github.com/catchorg/Catch2) v3, fetched automatically at configure time. Test discovery is registered with CTest, so each `TEST_CASE` runs as an individual test.

## Use it in your project

### As a subdirectory

```cmake
add_subdirectory(third_party/wcore)

target_link_libraries(my_app PRIVATE wcore::wcore)
```

### With FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(wcore
	GIT_REPOSITORY <repo-url>
	GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(wcore)

target_link_libraries(my_app PRIVATE wcore::wcore)
```

In both cases the include directories come from the target. Include public headers by their path under `<wcore/...>`:

```cpp
#include <wcore/log.hpp>
#include <wcore/types.hpp>
#include <wcore/version.hpp>
```

## Example

```cpp
#include <wcore/types.hpp>
#include <wcore/version.hpp>

#include <iostream>

int main()
{
	u32 width{ 1920 };
	f64 aspect{ 16.0 / 9.0 };

	std::cout << "wcore " << wcore::CurrentVersion << '\n';
	std::cout << width << " x " << width / aspect << '\n';
}
```

```text
wcore 0.1.0
1920 x 1080
```

## Logging

`wcore` uses [spdlog](https://github.com/gabime/spdlog) and ships it through the `wcore::wcore` target, so logging needs no extra setup. Initialize the logger once, near the start of `main`:

```cpp
#include <wcore/log.hpp>

int main()
{
	wcore::log::init();

	WCORE_INFO("engine started");
	WCORE_WARN("using {} worker threads", 4);
}
```

The default configuration logs to the console at `info` level. Use `wcore::log::Config` to add a file, enable rotation, or change the threshold:

```cpp
wcore::log::Config config;
config.minLevel = wcore::log::Level::debug;
config.file = "logs/engine.log";
config.rotate = true;
config.maxFileSize = 5 * 1024 * 1024;
config.maxFiles = 3;

if (!wcore::log::init(config))
{
	// A requested sink could not be created; logging continues with the rest.
}
```

Log through the `WCORE_TRACE` through `WCORE_CRITICAL` macros. Debug builds include trace and debug messages; release builds compile them out and start at info. Both `init` and the macros go through spdlog's default logger, so custom spdlog sinks and loggers remain compatible. Call `wcore::log::flush()` before teardown, or `wcore::log::shutdown()` to flush and release the sinks.

Headers are opt-in: include only the modules you use, and only those need to compile in a given translation unit.