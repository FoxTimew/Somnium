// Records the gameplay's logical model request without performing I/O.
// Runtime resolves the accepted request through the asset manager.
#include <somnium/core/logical_path.hpp>
#include <somnium/runtime/gameplay.hpp>
namespace somnium::runtime {
Result<void> GameContext::requestModel(std::string_view logicalPath) {
    auto path = core::logicalPath(logicalPath);
    if (!path)
        return tl::unexpected(path.error());
    m_modelPath = std::move(*path);
    return {};
}
} // namespace somnium::runtime
