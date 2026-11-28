// Marks the engine's function boundary explicitly in the iteration shared image.
// Static presets keep the same declarations without imports or exported data.
#pragma once

#if defined(SOMNIUM_ENGINE_SHARED)
#if defined(_WIN32)
#if defined(SOMNIUM_BUILDING_ENGINE)
#define SOMNIUM_ENGINE_API __declspec(dllexport)
#else
#define SOMNIUM_ENGINE_API __declspec(dllimport)
#endif
#define SOMNIUM_ENGINE_TYPE
#else
#define SOMNIUM_ENGINE_API __attribute__((visibility("default")))
// Polymorphic value types need visible vtables on ELF; no mutable data is exported.
#define SOMNIUM_ENGINE_TYPE __attribute__((visibility("default")))
#endif
#else
#define SOMNIUM_ENGINE_API
#define SOMNIUM_ENGINE_TYPE
#endif
