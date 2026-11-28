// Marks the C entry point of a gameplay image and its negative test fixture.
// Engine exports remain governed by their separate shared-library API.
#pragma once

#if defined(_WIN32)
#define SOMNIUM_GAME_EXPORT __declspec(dllexport)
#else
#define SOMNIUM_GAME_EXPORT __attribute__((visibility("default")))
#endif
