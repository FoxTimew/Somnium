// Characterizes the current Euler boundary independently of the reload protocol.
// Small angular error is acceptable here; exact snapshot restoration must avoid this conversion.
#include <bit>
#include <cmath>
#include <iostream>
#include <somnium/scene/rotation.hpp>

int main() {
    const auto original = glm::normalize(glm::quat(.31F, .47F, .63F, .59F));
    const auto first = somnium::scene::eulerDegrees(original);
    const auto restored = somnium::scene::rotationDegrees(first);
    const auto second = somnium::scene::eulerDegrees(restored);
    bool quaternionExact = true;
    bool eulerStable = true;
    for (int axis = 0; axis < 4; ++axis)
        quaternionExact &= std::bit_cast<std::uint32_t>(original[axis]) ==
                           std::bit_cast<std::uint32_t>(restored[axis]);
    for (int axis = 0; axis < 3; ++axis)
        eulerStable &=
            std::bit_cast<std::uint32_t>(first[axis]) == std::bit_cast<std::uint32_t>(second[axis]);
    std::cout << "Euler boundary: quaternion_bit_exact=" << quaternionExact
              << " canonical_euler_stable=" << eulerStable << '\n';
    // This audit reports exactness rather than pretending to implement snapshots.
    // It still rejects a physically different rotation or a non-finite result.
    const auto error = std::abs(1.F - std::abs(glm::dot(original, restored)));
    return std::isfinite(error) && error < 1.e-5F ? 0 : 1;
}
