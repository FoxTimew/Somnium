// Keeps Tracy types and instrumentation behind engine-owned macros.
// Disabled instrumentation never evaluates arguments or allocates.
#pragma once

#ifdef SOMNIUM_ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#define SOMNIUM_PROFILE_SCOPE(name) ZoneScopedN(name)
#define SOMNIUM_PROFILE_FRAME() FrameMark
#define SOMNIUM_PROFILE_ALLOC(pointer, size) TracyAlloc(pointer, size)
#define SOMNIUM_PROFILE_FREE(pointer) TracyFree(pointer)
#else
#define SOMNIUM_PROFILE_SCOPE(name) ((void)0)
#define SOMNIUM_PROFILE_FRAME() ((void)0)
#define SOMNIUM_PROFILE_ALLOC(pointer, size) ((void)0)
#define SOMNIUM_PROFILE_FREE(pointer) ((void)0)
#endif
