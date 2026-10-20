// Provides typed resource identities and generation-checked slot ownership.
// Slots retire before generation wrap so stale handles can never become live again.
#pragma once

#include <somnium/core/conventions.hpp>
#include <somnium/core/result.hpp>

#include <cstdint>
#include <limits>
#include <memory_resource>
#include <vector>

namespace somnium {
template <typename Tag> struct Handle {
    std::uint32_t index = core::kInvalidHandleIndex;
    std::uint32_t generation = core::kInvalidGeneration;

    [[nodiscard]] explicit constexpr operator bool() const {
        return index != core::kInvalidHandleIndex && generation != core::kInvalidGeneration;
    }
    friend constexpr bool operator==(Handle, Handle) = default;
};

// Owns identities only; the resource owner keeps its storage separate.
// Capacity growth is allowed during resource loading, never hidden in rendering.
template <typename Tag> class HandlePool {
  public:
    explicit HandlePool(std::pmr::memory_resource* memory = std::pmr::get_default_resource())
        : m_slots(memory) {}

    HandlePool(const HandlePool&) = delete;
    HandlePool& operator=(const HandlePool&) = delete;

    [[nodiscard]] Result<Handle<Tag>> acquire() {
        if (m_freeHead != core::kInvalidHandleIndex) {
            const auto index = m_freeHead;
            auto& slot = m_slots[index];
            m_freeHead = slot.next;
            slot.occupied = true;
            return Handle<Tag>{index, slot.generation};
        }
        if (m_slots.size() >= core::kInvalidHandleIndex) {
            return tl::unexpected(Error{ErrorCode::CapacityExceeded, "Handle pool exhausted"});
        }
        const auto index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.push_back(Slot{});
        return Handle<Tag>{index, core::kFirstGeneration};
    }

    [[nodiscard]] bool contains(Handle<Tag> handle) const {
        return handle.index < m_slots.size() && m_slots[handle.index].occupied &&
               m_slots[handle.index].generation == handle.generation;
    }

    [[nodiscard]] Result<void> release(Handle<Tag> handle) {
        if (!contains(handle)) {
            return tl::unexpected(Error{ErrorCode::InvalidHandle, "Stale or invalid handle"});
        }
        auto& slot = m_slots[handle.index];
        slot.occupied = false;
        if (slot.generation != std::numeric_limits<std::uint32_t>::max()) {
            ++slot.generation;
            slot.next = m_freeHead;
            m_freeHead = handle.index;
        }
        return {};
    }

  private:
    struct Slot {
        std::uint32_t generation = core::kFirstGeneration;
        std::uint32_t next = core::kInvalidHandleIndex;
        bool occupied = true;
    };
    std::pmr::vector<Slot> m_slots;
    std::uint32_t m_freeHead = core::kInvalidHandleIndex;
};
} // namespace somnium
