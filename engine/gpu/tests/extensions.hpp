// GPU-side contract proof, run before the unchanged D5 scene on every backend.
// Reads only installed shader artifacts through the injected reader.
#pragma once
#include <somnium/gpu/device.hpp>
somnium::Result<void> checkExtensions(somnium::gpu::Device& device,
                                      const somnium::core::ByteReader& shaders);
