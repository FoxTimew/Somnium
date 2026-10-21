// Implements the offscreen graphics contract with the pinned SDL GPU API.
// CPU recording never escapes a call; SDL owns deferred native destruction after submission.
#include <somnium/gpu/device.hpp>

#include <somnium/core/assert.hpp>
#include <somnium/core/profile.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_properties.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include <type_traits>
#include <utility>

namespace somnium::gpu {
namespace {
static_assert(std::is_standard_layout_v<Vertex> && sizeof(Vertex) == 9 * sizeof(float));
static_assert(offsetof(Vertex, position) == 0 && offsetof(Vertex, color) == 3 * sizeof(float) &&
              offsetof(Vertex, uv) == 7 * sizeof(float));
Error nativeError() {
    return {ErrorCode::Gpu, SDL_GetError()};
}
Error invalidHandle() {
    return {ErrorCode::InvalidHandle, "Stale or invalid GPU handle"};
}
Error invalidArgument() {
    return {ErrorCode::InvalidArgument, "Invalid GPU resource or pass description"};
}

template <typename Tag, typename Native, typename Metadata> struct Resources {
    struct Entry {
        Native* native = nullptr;
        Metadata metadata{};
    };
    HandlePool<Tag> identities;
    std::vector<Entry> entries;
    Result<Handle<Tag>> insert(Native* native, Metadata metadata) {
        auto handle = identities.acquire();
        if (!handle) {
            return tl::unexpected(handle.error());
        }
        if (handle->index == entries.size()) {
            entries.emplace_back();
        }
        entries[handle->index] = {native, std::move(metadata)};
        return handle;
    }
    Entry* get(Handle<Tag> handle) {
        return identities.contains(handle) ? &entries[handle.index] : nullptr;
    }
    template <typename Release>
    Result<void> erase(Handle<Tag> handle, SDL_GPUDevice* device, Release release) {
        auto* entry = get(handle);
        if (!entry) {
            return tl::unexpected(invalidHandle());
        }
        auto* native = std::exchange(entry->native, nullptr);
        auto result = identities.release(handle);
        release(device, native);
        return result;
    }
    template <typename Release> void clear(SDL_GPUDevice* device, Release release) {
        for (auto& entry : entries) {
            if (entry.native) {
                release(device, entry.native);
                entry.native = nullptr;
            }
        }
    }
};
struct ShaderMetadata {
    ShaderStage stage{};
    std::uint32_t samplers{}, storageTextures{}, storageBuffers{}, uniforms{};
};
struct Empty {};

struct Transfer {
    SDL_GPUDevice* device;
    SDL_GPUTransferBuffer* native;
    Transfer(SDL_GPUDevice* owner, SDL_GPUTransferBuffer* buffer) : device(owner), native(buffer) {}
    Transfer(const Transfer&) = delete;
    Transfer& operator=(const Transfer&) = delete;
    ~Transfer() {
        if (native) {
            SDL_ReleaseGPUTransferBuffer(device, native);
        }
    }
};
struct Command {
    SDL_GPUCommandBuffer* native;
    explicit Command(SDL_GPUCommandBuffer* buffer) : native(buffer) {}
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    ~Command() {
        if (native) {
            SDL_CancelGPUCommandBuffer(native);
        }
    }
    Result<void> submit() {
        auto* command = std::exchange(native, nullptr);
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            return tl::unexpected(nativeError());
        }
        return {};
    }
};
std::uint32_t rowPitch(std::uint32_t width) {
    return (width * 4U + 255U) & ~255U;
}
SDL_GPUTextureRegion region(SDL_GPUTexture* texture, TextureDescription description) {
    SDL_GPUTextureRegion result{};
    result.texture = texture;
    result.w = description.width;
    result.h = description.height;
    result.d = 1;
    return result;
}
SDL_GPUShaderFormat nativeFormat(ShaderFormat format) {
    switch (format) {
    case ShaderFormat::Spirv:
        return SDL_GPU_SHADERFORMAT_SPIRV;
    case ShaderFormat::Dxil:
        return SDL_GPU_SHADERFORMAT_DXIL;
    case ShaderFormat::Msl:
        return SDL_GPU_SHADERFORMAT_MSL;
    }
    return SDL_GPU_SHADERFORMAT_INVALID;
}
} // namespace

struct Device::Impl {
    SDL_GPUDevice* device = nullptr;
    DeviceInfo information{};
    std::thread::id owner = std::this_thread::get_id();
    Resources<BufferTag, SDL_GPUBuffer, std::uint32_t> buffers;
    Resources<TextureTag, SDL_GPUTexture, TextureDescription> textures;
    Resources<SamplerTag, SDL_GPUSampler, Empty> samplers;
    Resources<ShaderTag, SDL_GPUShader, ShaderMetadata> shaders;
    Resources<PipelineTag, SDL_GPUGraphicsPipeline, Empty> pipelines;
    void checkThread() const {
        SOMNIUM_VERIFY(owner == std::this_thread::get_id());
    }
    ~Impl() {
        checkThread();
        if (device) {
            SDL_WaitForGPUIdle(device);
            pipelines.clear(device, SDL_ReleaseGPUGraphicsPipeline);
            shaders.clear(device, SDL_ReleaseGPUShader);
            samplers.clear(device, SDL_ReleaseGPUSampler);
            textures.clear(device, SDL_ReleaseGPUTexture);
            buffers.clear(device, SDL_ReleaseGPUBuffer);
            SDL_DestroyGPUDevice(device);
        }
    }
};

Device::Device(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {
    SOMNIUM_PROFILE_ALLOC(this, sizeof(Device));
}
Device::~Device() {
    SOMNIUM_PROFILE_FREE(this);
}
const DeviceInfo& Device::info() const {
    m_impl->checkThread();
    return m_impl->information;
}

Result<std::unique_ptr<Device>> Device::create(DeviceConfig config) {
    const char* driver = nullptr;
    switch (config.backend) {
    case Backend::Vulkan:
        driver = "vulkan";
        break;
    case Backend::D3D12:
        driver = "direct3d12";
        break;
    case Backend::Metal:
        driver = "metal";
        break;
    default:
        return tl::unexpected(invalidArgument());
    }
    auto impl = std::make_unique<Impl>();
    const auto properties = SDL_CreateProperties();
    if (!properties) {
        return tl::unexpected(nativeError());
    }
    const bool configured =
        SDL_SetStringProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, driver) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,
                               config.debug) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, true) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,
                               true) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true) &&
        SDL_SetBooleanProperty(properties,
                               SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false);
    if (configured) {
        impl->device = SDL_CreateGPUDeviceWithProperties(properties);
    }
    const auto error = nativeError();
    SDL_DestroyProperties(properties);
    if (!impl->device) {
        return tl::unexpected(error);
    }
    const char* selected = SDL_GetGPUDeviceDriver(impl->device);
    if (!selected || std::strcmp(driver, selected) != 0) {
        return tl::unexpected(
            Error{ErrorCode::Gpu, "Requested GPU driver overridden; refusing silent fallback"});
    }
    const auto formats = SDL_GetGPUShaderFormats(impl->device);
    ShaderFormat format;
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        format = ShaderFormat::Spirv;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        format = ShaderFormat::Dxil;
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        format = ShaderFormat::Msl;
    } else {
        return tl::unexpected(Error{ErrorCode::Gpu, "No offline shader format supported"});
    }
    const auto details = SDL_GetGPUDeviceProperties(impl->device);
    impl->information = {
        selected, SDL_GetStringProperty(details, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown"),
        SDL_GetStringProperty(details, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "unknown"),
        format};
    return std::unique_ptr<Device>(new Device(std::move(impl)));
}

Result<Buffer> Device::createVertexBuffer(std::uint32_t bytes) {
    m_impl->checkThread();
    if (!bytes || bytes % sizeof(Vertex) != 0) {
        return tl::unexpected(invalidArgument());
    }
    SDL_GPUBufferCreateInfo description{};
    description.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    description.size = bytes;
    auto* native = SDL_CreateGPUBuffer(m_impl->device, &description);
    if (!native) {
        return tl::unexpected(nativeError());
    }
    auto handle = m_impl->buffers.insert(native, bytes);
    if (!handle) {
        SDL_ReleaseGPUBuffer(m_impl->device, native);
    }
    return handle;
}

Result<Texture> Device::createTexture(TextureDescription description) {
    m_impl->checkThread();
    if (!description.width || !description.height || description.width > 16384 ||
        description.height > 16384 || !std::isfinite(description.clearDepth) ||
        description.clearDepth < 0 || description.clearDepth > 1 ||
        !std::all_of(description.clearColor.begin(), description.clearColor.end(),
                     [](float value) { return std::isfinite(value); })) {
        return tl::unexpected(invalidArgument());
    }
    SDL_GPUTextureCreateInfo nativeDescription{};
    nativeDescription.type = SDL_GPU_TEXTURETYPE_2D;
    nativeDescription.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    switch (description.kind) {
    case TextureKind::SampledRgba8:
        nativeDescription.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        break;
    case TextureKind::ColorRgba8:
        nativeDescription.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        break;
    case TextureKind::Depth32:
        nativeDescription.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        nativeDescription.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        break;
    default:
        return tl::unexpected(invalidArgument());
    }
    nativeDescription.width = description.width;
    nativeDescription.height = description.height;
    nativeDescription.layer_count_or_depth = 1;
    nativeDescription.num_levels = 1;
    nativeDescription.sample_count = SDL_GPU_SAMPLECOUNT_1;
    if (!SDL_GPUTextureSupportsFormat(m_impl->device, nativeDescription.format,
                                      nativeDescription.type, nativeDescription.usage)) {
        return tl::unexpected(Error{ErrorCode::Gpu, "Required texture format unsupported"});
    }
    const auto properties = SDL_CreateProperties();
    if (!properties) {
        return tl::unexpected(nativeError());
    }
    const bool configured =
        SDL_SetFloatProperty(properties, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_R_FLOAT,
                             description.clearColor[0]) &&
        SDL_SetFloatProperty(properties, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_G_FLOAT,
                             description.clearColor[1]) &&
        SDL_SetFloatProperty(properties, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_B_FLOAT,
                             description.clearColor[2]) &&
        SDL_SetFloatProperty(properties, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT,
                             description.clearColor[3]) &&
        SDL_SetFloatProperty(properties, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_DEPTH_FLOAT,
                             description.clearDepth);
    nativeDescription.props = properties;
    auto* native = configured ? SDL_CreateGPUTexture(m_impl->device, &nativeDescription) : nullptr;
    const auto error = nativeError();
    SDL_DestroyProperties(properties);
    if (!native) {
        return tl::unexpected(error);
    }
    auto handle = m_impl->textures.insert(native, description);
    if (!handle) {
        SDL_ReleaseGPUTexture(m_impl->device, native);
    }
    return handle;
}

Result<Sampler> Device::createNearestSampler() {
    m_impl->checkThread();
    SDL_GPUSamplerCreateInfo description{};
    description.min_filter = description.mag_filter = SDL_GPU_FILTER_NEAREST;
    description.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    description.address_mode_u = description.address_mode_v = description.address_mode_w =
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    auto* native = SDL_CreateGPUSampler(m_impl->device, &description);
    if (!native) {
        return tl::unexpected(nativeError());
    }
    auto handle = m_impl->samplers.insert(native, {});
    if (!handle) {
        SDL_ReleaseGPUSampler(m_impl->device, native);
    }
    return handle;
}

Result<Shader> Device::createShader(const ShaderDescription& description) {
    m_impl->checkThread();
    const bool vertex = description.stage == ShaderStage::Vertex;
    if ((!vertex && description.stage != ShaderStage::Fragment) || description.code.empty() ||
        description.entrypoint != (vertex ? "vsMain" : "psMain") ||
        !(nativeFormat(description.format) & SDL_GetGPUShaderFormats(m_impl->device)) ||
        description.samplers > 16 || description.storageTextures > 8 ||
        description.storageBuffers > 8 || description.uniformBuffers > 4) {
        return tl::unexpected(invalidArgument());
    }
    // SDL Metal uses initWithBytes:length:; preserve the exact offline text bytes.
    const auto code = description.code;
    SDL_GPUShaderCreateInfo nativeDescription{};
    nativeDescription.code = reinterpret_cast<const Uint8*>(code.data());
    nativeDescription.code_size = code.size();
    nativeDescription.entrypoint = description.entrypoint.c_str();
    nativeDescription.format = nativeFormat(description.format);
    nativeDescription.stage = vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    nativeDescription.num_samplers = description.samplers;
    nativeDescription.num_storage_textures = description.storageTextures;
    nativeDescription.num_storage_buffers = description.storageBuffers;
    nativeDescription.num_uniform_buffers = description.uniformBuffers;
    auto* native = SDL_CreateGPUShader(m_impl->device, &nativeDescription);
    if (!native) {
        return tl::unexpected(nativeError());
    }
    auto handle = m_impl->shaders.insert(
        native, {description.stage, description.samplers, description.storageTextures,
                 description.storageBuffers, description.uniformBuffers});
    if (!handle) {
        SDL_ReleaseGPUShader(m_impl->device, native);
    }
    return handle;
}

Result<Pipeline> Device::createPipeline(Shader vertex, Shader fragment) {
    m_impl->checkThread();
    const auto* vs = m_impl->shaders.get(vertex);
    const auto* ps = m_impl->shaders.get(fragment);
    if (!vs || !ps) {
        return tl::unexpected(invalidHandle());
    }
    if (vs->metadata.stage != ShaderStage::Vertex || ps->metadata.stage != ShaderStage::Fragment ||
        vs->metadata.samplers || vs->metadata.storageTextures || vs->metadata.storageBuffers ||
        vs->metadata.uniforms || ps->metadata.samplers != 1 || ps->metadata.storageTextures ||
        ps->metadata.storageBuffers || ps->metadata.uniforms) {
        return tl::unexpected(invalidArgument());
    }
    const SDL_GPUVertexBufferDescription buffer{0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX,
                                                0};
    const SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, position)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, color)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, uv)}};
    SDL_GPUColorTargetDescription color{};
    color.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo description{};
    description.vertex_shader = vs->native;
    description.fragment_shader = ps->native;
    description.vertex_input_state = {&buffer, 1, attributes, 3};
    description.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    description.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    description.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    description.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    description.rasterizer_state.enable_depth_clip = true;
    description.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    description.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    description.depth_stencil_state.enable_depth_test = true;
    description.depth_stencil_state.enable_depth_write = true;
    description.target_info.color_target_descriptions = &color;
    description.target_info.num_color_targets = 1;
    description.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    description.target_info.has_depth_stencil_target = true;
    auto* native = SDL_CreateGPUGraphicsPipeline(m_impl->device, &description);
    if (!native) {
        return tl::unexpected(nativeError());
    }
    auto handle = m_impl->pipelines.insert(native, {});
    if (!handle) {
        SDL_ReleaseGPUGraphicsPipeline(m_impl->device, native);
    }
    return handle;
}

Result<void> Device::upload(Buffer buffer, std::span<const std::byte> bytes) {
    m_impl->checkThread();
    const auto* entry = m_impl->buffers.get(buffer);
    if (!entry) {
        return tl::unexpected(invalidHandle());
    }
    if (bytes.size() != entry->metadata) {
        return tl::unexpected(invalidArgument());
    }
    const SDL_GPUTransferBufferCreateInfo description{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                      entry->metadata, 0};
    Transfer transfer{m_impl->device, SDL_CreateGPUTransferBuffer(m_impl->device, &description)};
    if (!transfer.native) {
        return tl::unexpected(nativeError());
    }
    auto* mapped = SDL_MapGPUTransferBuffer(m_impl->device, transfer.native, true);
    if (!mapped) {
        return tl::unexpected(nativeError());
    }
    std::memcpy(mapped, bytes.data(), bytes.size());
    SDL_UnmapGPUTransferBuffer(m_impl->device, transfer.native);
    Command command{SDL_AcquireGPUCommandBuffer(m_impl->device)};
    if (!command.native) {
        return tl::unexpected(nativeError());
    }
    auto* pass = SDL_BeginGPUCopyPass(command.native);
    if (!pass) {
        return tl::unexpected(nativeError());
    }
    const SDL_GPUTransferBufferLocation source{transfer.native, 0};
    const SDL_GPUBufferRegion destination{entry->native, 0, entry->metadata};
    SDL_UploadToGPUBuffer(pass, &source, &destination, true);
    SDL_EndGPUCopyPass(pass);
    return command.submit();
}

Result<void> Device::upload(Texture texture, std::span<const std::byte> rgba) {
    m_impl->checkThread();
    const auto* entry = m_impl->textures.get(texture);
    if (!entry) {
        return tl::unexpected(invalidHandle());
    }
    const auto description = entry->metadata;
    if (description.kind != TextureKind::SampledRgba8 ||
        rgba.size() != std::size_t(description.width) * description.height * 4) {
        return tl::unexpected(invalidArgument());
    }
    // SDL 3.4.18 Metal uploads derive their stride from the destination width,
    // ignoring pixels_per_row. Other backends retain D3D12's 256-byte alignment.
    const auto pitch = m_impl->information.driver == "metal" ? description.width * 4U
                                                             : rowPitch(description.width);
    const SDL_GPUTransferBufferCreateInfo transferDescription{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                              pitch * description.height, 0};
    Transfer transfer{m_impl->device,
                      SDL_CreateGPUTransferBuffer(m_impl->device, &transferDescription)};
    if (!transfer.native) {
        return tl::unexpected(nativeError());
    }
    auto* mapped =
        static_cast<std::byte*>(SDL_MapGPUTransferBuffer(m_impl->device, transfer.native, true));
    if (!mapped) {
        return tl::unexpected(nativeError());
    }
    for (std::uint32_t y = 0; y < description.height; ++y) {
        std::memcpy(mapped + std::size_t(y) * pitch,
                    rgba.data() + std::size_t(y) * description.width * 4, description.width * 4);
    }
    SDL_UnmapGPUTransferBuffer(m_impl->device, transfer.native);
    Command command{SDL_AcquireGPUCommandBuffer(m_impl->device)};
    if (!command.native) {
        return tl::unexpected(nativeError());
    }
    auto* pass = SDL_BeginGPUCopyPass(command.native);
    if (!pass) {
        return tl::unexpected(nativeError());
    }
    const SDL_GPUTextureTransferInfo source{transfer.native, 0, pitch / 4, description.height};
    const auto destination = region(entry->native, description);
    SDL_UploadToGPUTexture(pass, &source, &destination, true);
    SDL_EndGPUCopyPass(pass);
    return command.submit();
}

Result<void> Device::render(const RenderPass& description, std::span<const Draw> draws) {
    m_impl->checkThread();
    SOMNIUM_PROFILE_SCOPE("gpu.offscreen");
    const auto* color = m_impl->textures.get(description.color);
    const auto* depth = m_impl->textures.get(description.depth);
    if (!color || !depth) {
        return tl::unexpected(invalidHandle());
    }
    if (color->metadata.kind != TextureKind::ColorRgba8 ||
        depth->metadata.kind != TextureKind::Depth32 ||
        color->metadata.width != depth->metadata.width ||
        color->metadata.height != depth->metadata.height ||
        !std::isfinite(description.clearDepth) || description.clearDepth < 0 ||
        description.clearDepth > 1 ||
        !std::all_of(description.clearColor.begin(), description.clearColor.end(),
                     [](float value) { return std::isfinite(value); })) {
        return tl::unexpected(invalidArgument());
    }
    // Validate the entire list before recording, so errors cannot leave a partial pass.
    for (const auto& draw : draws) {
        const auto* buffer = m_impl->buffers.get(draw.vertices);
        const auto* texture = m_impl->textures.get(draw.texture);
        if (!buffer || !texture || !m_impl->pipelines.get(draw.pipeline) ||
            !m_impl->samplers.get(draw.sampler)) {
            return tl::unexpected(invalidHandle());
        }
        const auto capacity = buffer->metadata / sizeof(Vertex);
        if (texture->metadata.kind != TextureKind::SampledRgba8 || draw.firstVertex > capacity ||
            draw.vertexCount > capacity - draw.firstVertex || !draw.vertexCount ||
            draw.vertexCount % 3 != 0) {
            return tl::unexpected(invalidArgument());
        }
    }
    Command command{SDL_AcquireGPUCommandBuffer(m_impl->device)};
    if (!command.native) {
        return tl::unexpected(nativeError());
    }
    SDL_PushGPUDebugGroup(command.native, "Somnium offscreen");
    SDL_GPUColorTargetInfo colorTarget{};
    colorTarget.texture = color->native;
    colorTarget.clear_color = {description.clearColor[0], description.clearColor[1],
                               description.clearColor[2], description.clearColor[3]};
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    colorTarget.cycle = true;
    SDL_GPUDepthStencilTargetInfo depthTarget{};
    depthTarget.texture = depth->native;
    depthTarget.clear_depth = description.clearDepth;
    depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depthTarget.cycle = true;
    auto* pass = SDL_BeginGPURenderPass(command.native, &colorTarget, 1, &depthTarget);
    if (!pass) {
        SDL_PopGPUDebugGroup(command.native);
        return tl::unexpected(nativeError());
    }
    for (const auto& draw : draws) {
        SDL_BindGPUGraphicsPipeline(pass, m_impl->pipelines.get(draw.pipeline)->native);
        const SDL_GPUBufferBinding vertices{m_impl->buffers.get(draw.vertices)->native, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
        const SDL_GPUTextureSamplerBinding texture{m_impl->textures.get(draw.texture)->native,
                                                   m_impl->samplers.get(draw.sampler)->native};
        SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
        SDL_DrawGPUPrimitives(pass, draw.vertexCount, 1, draw.firstVertex, 0);
    }
    SDL_EndGPURenderPass(pass);
    SDL_PopGPUDebugGroup(command.native);
    return command.submit();
}

Result<std::vector<std::byte>> Device::readback(Texture texture) {
    m_impl->checkThread();
    const auto* entry = m_impl->textures.get(texture);
    if (!entry) {
        return tl::unexpected(invalidHandle());
    }
    const auto description = entry->metadata;
    if (description.kind != TextureKind::ColorRgba8) {
        return tl::unexpected(invalidArgument());
    }
    const auto pitch = rowPitch(description.width);
    const SDL_GPUTransferBufferCreateInfo transferDescription{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                                              pitch * description.height, 0};
    Transfer transfer{m_impl->device,
                      SDL_CreateGPUTransferBuffer(m_impl->device, &transferDescription)};
    if (!transfer.native) {
        return tl::unexpected(nativeError());
    }
    Command command{SDL_AcquireGPUCommandBuffer(m_impl->device)};
    if (!command.native) {
        return tl::unexpected(nativeError());
    }
    auto* pass = SDL_BeginGPUCopyPass(command.native);
    if (!pass) {
        return tl::unexpected(nativeError());
    }
    const auto source = region(entry->native, description);
    const SDL_GPUTextureTransferInfo destination{transfer.native, 0, pitch / 4, description.height};
    SDL_DownloadFromGPUTexture(pass, &source, &destination);
    SDL_EndGPUCopyPass(pass);
    auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(std::exchange(command.native, nullptr));
    if (!fence) {
        return tl::unexpected(nativeError());
    }
    const bool complete = SDL_WaitForGPUFences(m_impl->device, true, &fence, 1);
    const auto error = nativeError();
    SDL_ReleaseGPUFence(m_impl->device, fence);
    if (!complete) {
        return tl::unexpected(error);
    }
    const auto* mapped = static_cast<const std::byte*>(
        SDL_MapGPUTransferBuffer(m_impl->device, transfer.native, false));
    if (!mapped) {
        return tl::unexpected(nativeError());
    }
    std::vector<std::byte> result(std::size_t(description.width) * description.height * 4);
    for (std::uint32_t y = 0; y < description.height; ++y) {
        std::memcpy(result.data() + std::size_t(y) * description.width * 4,
                    mapped + std::size_t(y) * pitch, description.width * 4);
    }
    SDL_UnmapGPUTransferBuffer(m_impl->device, transfer.native);
    return result;
}

Result<void> Device::destroy(Buffer handle) {
    m_impl->checkThread();
    return m_impl->buffers.erase(handle, m_impl->device, SDL_ReleaseGPUBuffer);
}
Result<void> Device::destroy(Texture handle) {
    m_impl->checkThread();
    return m_impl->textures.erase(handle, m_impl->device, SDL_ReleaseGPUTexture);
}
Result<void> Device::destroy(Shader handle) {
    m_impl->checkThread();
    return m_impl->shaders.erase(handle, m_impl->device, SDL_ReleaseGPUShader);
}
Result<void> Device::destroy(Sampler handle) {
    m_impl->checkThread();
    return m_impl->samplers.erase(handle, m_impl->device, SDL_ReleaseGPUSampler);
}
Result<void> Device::destroy(Pipeline handle) {
    m_impl->checkThread();
    return m_impl->pipelines.erase(handle, m_impl->device, SDL_ReleaseGPUGraphicsPipeline);
}
} // namespace somnium::gpu
