#pragma once

#include <cstdio>
#include <cstdlib>

// Internal Implementation (Do not use directly).
#define WCORE_ASSERT_FAIL(expr, msg)                        \
    do {                                                    \
        std::fprintf(stderr,                                \
            "\n[WCORE ASSERT] %s\n"                         \
            "  expression: %s\n"                            \
            "  function: %s\n"                              \
            "  file: %s:%d\n\n",                            \
            (msg), #expr, __func__, __FILE__, __LINE__);    \
        std::abort();                                       \
    } while(0)

// Enabled in debug and release, 
// Use for invariants that must never be violated.
#define WCORE_ASSERT(expr, msg)                            \
    do {                                                   \
        if (!(expr)) { WCORE_ASSERT_FAIL(expr, msg); }     \
    } while(0)

// Stripped in release,
// Use for performance-sensitive checks.
#ifdef NDEBUG
#define WCORE_ASSERT_DEBUG(expr, msg) do {} while(0)
#else
#define WCORE_ASSERT_DEBUG(expr, msg) WCORE_ASSERT(expr, msg)
#endif

// Marks code paths that should never execute.
#define WCORE_UNREACHABLE(msg) WCORE_ASSERT_FAIL(unreachable, msg)