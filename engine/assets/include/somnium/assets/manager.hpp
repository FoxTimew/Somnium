// Owns immutable CPU assets and typed, generation-checked identities per loader type.
// Callers request then inspect state; synchronous completion is not an API guarantee.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <somnium/core/byte_reader.hpp>
#include <somnium/core/handle.hpp>
#include <somnium/core/logical_path.hpp>
#include <utility>
#include <vector>

namespace somnium::assets {
enum class State : std::uint8_t { Unloaded, Loading, Ready, Failed };
template <typename T> struct AssetTag {};
template <typename T> using AssetHandle = Handle<AssetTag<T>>;
template <typename T> class Loader {
  public:
    virtual ~Loader() = default;
    [[nodiscard]] virtual Result<T> load(const core::ByteReader& reader,
                                         std::string_view path) const = 0;
};
template <typename T> class AssetManager final {
  public:
    AssetManager(const core::ByteReader& reader, const Loader<T>& loader)
        : m_reader(reader), m_loader(loader) {}
    // Identifying without loading exposes Unloaded and allows deferred requests.
    [[nodiscard]] Result<AssetHandle<T>> identify(std::string_view path) {
        auto canonical = core::logicalPath(path);
        if (!canonical)
            return tl::unexpected(canonical.error());
        const auto found = m_paths.find(*canonical);
        if (found != m_paths.end())
            return found->second;
        auto handle = m_handles.acquire();
        if (!handle)
            return tl::unexpected(handle.error());
        if (handle->index == m_entries.size())
            m_entries.emplace_back();
        auto entry = std::make_unique<Entry>();
        entry->path = std::move(*canonical);
        m_paths.emplace(entry->path, *handle);
        m_entries[handle->index] = std::move(entry);
        return *handle;
    }
    [[nodiscard]] Result<AssetHandle<T>> request(std::string_view path) {
        auto handle = identify(path);
        if (!handle)
            return handle;
        auto loaded = request(*handle);
        if (!loaded)
            return tl::unexpected(loaded.error());
        return *handle;
    }
    [[nodiscard]] Result<void> request(AssetHandle<T> handle) {
        auto* entry = find(handle);
        if (!entry)
            return tl::unexpected(stale());
        if (entry->state != State::Unloaded)
            return {};
        entry->state = State::Loading;
        auto data = m_loader.load(m_reader, entry->path);
        if (data) {
            entry->data = std::make_unique<T>(std::move(*data));
            entry->state = State::Ready;
        } else {
            entry->error = std::move(data.error());
            entry->state = State::Failed;
        }
        return {};
    }
    [[nodiscard]] Result<State> state(AssetHandle<T> handle) const {
        const auto* entry = find(handle);
        if (!entry)
            return tl::unexpected(stale());
        return entry->state;
    }
    // A short borrow, valid until explicit unload; never store it across that boundary.
    [[nodiscard]] Result<std::reference_wrapper<const T>> get(AssetHandle<T> handle) const {
        const auto* entry = find(handle);
        if (!entry)
            return tl::unexpected(stale());
        if (entry->state != State::Ready)
            return tl::unexpected(Error{ErrorCode::InvalidState, "Asset is not Ready"});
        return std::cref(*entry->data);
    }
    [[nodiscard]] Result<Error> error(AssetHandle<T> handle) const {
        const auto* entry = find(handle);
        if (!entry)
            return tl::unexpected(stale());
        if (!entry->error)
            return tl::unexpected(Error{ErrorCode::InvalidState, "Asset has no failure"});
        return *entry->error;
    }
    [[nodiscard]] Result<void> unload(AssetHandle<T> handle) {
        auto* entry = find(handle);
        if (!entry)
            return tl::unexpected(stale());
        if (entry->state == State::Loading)
            return tl::unexpected(Error{ErrorCode::InvalidState, "Cannot unload a loading asset"});
        m_paths.erase(entry->path);
        m_entries[handle.index].reset();
        return m_handles.release(handle);
    }

  private:
    struct Entry {
        std::string path;
        State state = State::Unloaded;
        std::unique_ptr<T> data;
        std::optional<Error> error;
    };
    [[nodiscard]] Entry* find(AssetHandle<T> handle) const {
        return m_handles.contains(handle) ? m_entries[handle.index].get() : nullptr;
    }
    static Error stale() {
        return {ErrorCode::InvalidHandle, "Stale asset handle"};
    }
    const core::ByteReader& m_reader;
    const Loader<T>& m_loader;
    HandlePool<AssetTag<T>> m_handles;
    std::vector<std::unique_ptr<Entry>> m_entries;
    std::map<std::string, AssetHandle<T>, std::less<>> m_paths;
};
} // namespace somnium::assets
