// Computes the host ABI in the single engine image, independent of the module's value.
// The fingerprint includes the conditional codec editor field and iteration option.
#include <somnium/runtime/module_abi.hpp>

namespace somnium::runtime {
std::string engineModuleAbi() {
    return compiledModuleAbi();
}
} // namespace somnium::runtime
