<div align="center">

# WCORE

### **Wink Core** - The Foundation

*One core library. Every Wink software. Every Wink system.*

<br>

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-build-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![Status](https://img.shields.io/badge/status-in%20development-orange?style=for-the-badge)

</div>

---

## What is WCORE?

**WCORE** (*Wink Core*) is the shared foundation of the Wink ecosystem. It is the core C++ library that every Wink application, tool and system builds on top of, so common building blocks are written **once**, tested **once**, and reused **everywhere**.

Instead of every Wink project reinventing its own utilities, WCORE provides a single, consistent, well-tested base layer.

> All public headers live under `include/wcore/`, so everything is included with the `wcore/` prefix.

## Getting Started

### Requirements

- **C++20 compiler** with `<format>` support: **MSVC 19.29+**,
  **GCC 13+** or **Clang 17+**
- **CMake** 3.21 or newer (the presets additionally need Ninja)
- **Ninja** (only when using the bundled CMake presets)

Tested on Windows with MSVC and on Linux with GCC 13+; the code only uses the
C++ standard library and treats paths with `std::filesystem`, so no platform
specific headers or APIs are required.

> Note: file paths are taken as narrow (`std::string_view`) strings. On Windows
> that means non-ASCII paths are only supported if they can be represented in
> the active code page.

### Build

```bash
# Configure
cmake -S . -B build

# Compile
cmake --build build --config Release
```

### Use it in your project

```cpp
#include <wcore/...>   // include the module you need

int main() {
    // build something amazing with Wink Core
}
```

## Testing

WCORE ships with a tiny, homegrown test framework: **`test.hpp`**. No heavy external dependencies, just include the header, write your tests, and let CMake/CTest run them.

```bash
cd build
ctest -C Release --output-on-failure
```

Each test file provides its own entry point and is registered with CTest through CMake, which keeps tests fast, isolated and easy to add.

## Contributing

Working on a Wink project and need something in the core?

1. Keep it **generic**: if it's useful to more than one Wink project, it belongs here.
2. Keep it **small and focused**: one clear responsibility per module.
3. Add a **test** alongside every new feature.
4. Keep the public API **clean**, stable and documented.

<br>
<br>

<div align="center">

**Made with ♥ for the Wink ecosystem**

*The project contains minimal, monitored AI written code*.

</div>