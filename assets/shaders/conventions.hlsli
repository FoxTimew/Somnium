// Defines the SDL graphics register spaces shared by every engine shader.
// Resource t-indices are dense: sampled textures, storage textures, storage buffers.
#ifndef SOMNIUM_SHADER_CONVENTIONS
#define SOMNIUM_SHADER_CONVENTIONS
#define SOMNIUM_REGISTER_JOIN_IMPL(prefix, index) prefix##index
#define SOMNIUM_REGISTER_JOIN(prefix, index) SOMNIUM_REGISTER_JOIN_IMPL(prefix, index)
#if defined(SOMNIUM_VERTEX)
#define SOMNIUM_RESOURCE_SPACE space0
#define SOMNIUM_UNIFORM_SPACE space1
#elif defined(SOMNIUM_FRAGMENT)
#define SOMNIUM_RESOURCE_SPACE space2
#define SOMNIUM_UNIFORM_SPACE space3
#else
#error The build must declare the shader stage
#endif
#define SOMNIUM_TEXTURE(index) register(SOMNIUM_REGISTER_JOIN(t, index), SOMNIUM_RESOURCE_SPACE)
#define SOMNIUM_STORAGE(index) register(SOMNIUM_REGISTER_JOIN(t, index), SOMNIUM_RESOURCE_SPACE)
#define SOMNIUM_SAMPLER(index) register(SOMNIUM_REGISTER_JOIN(s, index), SOMNIUM_RESOURCE_SPACE)
#define SOMNIUM_UNIFORM(index) register(SOMNIUM_REGISTER_JOIN(b, index), SOMNIUM_UNIFORM_SPACE)
#endif
