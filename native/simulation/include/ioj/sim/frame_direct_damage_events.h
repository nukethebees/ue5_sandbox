#pragma once

#include "ioj/sim/direct_damage_events.h"
#include "ioj/sim/entity_handle.h"
#include "sandbox/core/frame_array.h"

#include <cstdint>
#include <memory_resource>

namespace ioj::sim::lasers {
struct FrameDirectDamageEvents {
    explicit FrameDirectDamageEvents(std::pmr::memory_resource* resource);

    FrameDirectDamageEvents(FrameDirectDamageEvents const&) = delete;
    FrameDirectDamageEvents(FrameDirectDamageEvents&&) = delete;
    auto operator=(FrameDirectDamageEvents const&) -> FrameDirectDamageEvents& = delete;
    auto operator=(FrameDirectDamageEvents&&) -> FrameDirectDamageEvents& = delete;
    ~FrameDirectDamageEvents() = default;

    void reserve(std::int32_t count);
    void add(RegistryEntityHandle damaged_entity,
             std::int32_t damage_amount,
             RegistryEntityHandle instigator);
    [[nodiscard]] auto get_const_view() const noexcept -> DirectDamageEventsConstView;

    ml::FrameArray<RegistryEntityHandle> damaged_entities;
    ml::FrameArray<std::int32_t> damage_amounts;
    ml::FrameArray<RegistryEntityHandle> instigators;
};
} // namespace ioj::sim::lasers
