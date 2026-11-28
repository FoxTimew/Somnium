// Describes the local C++ ABI before any codec, factory or Gameplay call.
// Each image computes its own value; using the imported host value would hide incompatibility.
#pragma once
#include <somnium/runtime/module_build.hpp>
#include <somnium/scene/serialization.hpp>

namespace somnium::runtime {
[[nodiscard]] inline std::string compiledModuleAbi() {
    std::string value = SOMNIUM_ENGINE_BUILD_ID;
    value += ":codec=" + std::to_string(sizeof(scene::ComponentCodec)) + ":" +
             std::to_string(alignof(scene::ComponentCodec));
    value += ":registry=" + std::to_string(sizeof(entt::registry));
    value += ":json=" + std::to_string(sizeof(scene::Json));
    value += ":pointer=" + std::to_string(sizeof(void*));
#if defined(SOMNIUM_ENABLE_ITERATION)
    value += ":iteration=1";
#else
    value += ":iteration=0";
#endif
#if defined(_MSVC_STL_UPDATE)
    value += ":msvc-stl=" + std::to_string(_MSVC_STL_UPDATE);
    value += ":iterator=" + std::to_string(_ITERATOR_DEBUG_LEVEL);
#elif defined(_LIBCPP_VERSION)
    value += ":libc++=" + std::to_string(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
    value += ":libstdc++=" + std::to_string(__GLIBCXX__);
    value += ":cxx11=" + std::to_string(_GLIBCXX_USE_CXX11_ABI);
#endif
    return value;
}
[[nodiscard]] SOMNIUM_ENGINE_API std::string engineModuleAbi();
} // namespace somnium::runtime
