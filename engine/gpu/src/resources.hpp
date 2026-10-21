// Separates logical handle invalidation from native resource retirement.
// Recording borrows survive slot reuse until the owning command buffer finishes.
#pragma once
#include <somnium/core/handle.hpp>
#include <utility>
#include <vector>

namespace somnium::gpu::detail {
template <typename Tag, typename Native, typename Metadata> struct Resources {
    struct Entry {
        Native* native = nullptr;
        Metadata metadata{};
    };
    HandlePool<Tag> identities;
    std::vector<Entry> entries;
    std::vector<Native*> retired;
    Result<Handle<Tag>> insert(Native* native, Metadata metadata) {
        auto handle = identities.acquire();
        if (!handle)
            return tl::unexpected(handle.error());
        if (handle->index == entries.size())
            entries.emplace_back();
        entries[handle->index] = {native, std::move(metadata)};
        return handle;
    }
    Entry* get(Handle<Tag> handle) {
        return identities.contains(handle) ? &entries[handle.index] : nullptr;
    }
    template <typename Release>
    Result<void> erase(Handle<Tag> handle, bool recording, Release release) {
        auto* entry = get(handle);
        if (!entry)
            return tl::unexpected(Error{ErrorCode::InvalidHandle, "Stale GPU handle"});
        auto* native = std::exchange(entry->native, nullptr);
        auto result = identities.release(handle);
        if (recording)
            retired.push_back(native);
        else
            release(native);
        return result;
    }
    template <typename Release> void flush(Release release) {
        for (auto* native : retired)
            release(native);
        retired.clear();
    }
    template <typename Device, typename Release> void clear(Device* device, Release release) {
        flush([&](Native* native) { release(device, native); });
        for (auto& entry : entries) {
            if (entry.native)
                release(device, std::exchange(entry.native, nullptr));
        }
    }
};
} // namespace somnium::gpu::detail
