#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/direct_damage_events.h"
#include "sandbox/simulation/entity_handle.h"

#include <cstdint>
#include <memory_resource>

namespace ml::simulation::lasers {
struct FrameDirectDamageEvents {
    explicit FrameDirectDamageEvents(std::pmr::memory_resource* resource);

    FrameDirectDamageEvents(FrameDirectDamageEvents const&) = delete;
    FrameDirectDamageEvents(FrameDirectDamageEvents&&) = delete;
    auto operator=(FrameDirectDamageEvents const&) -> FrameDirectDamageEvents& = delete;
    auto operator=(FrameDirectDamageEvents&&) -> FrameDirectDamageEvents& = delete;
    ~FrameDirectDamageEvents() = default;

    void reserve(std::int32_t count);
    void add(FRegistryEntityHandle damaged_entity,
             std::int32_t damage_amount,
             FRegistryEntityHandle instigator);
    [[nodiscard]] auto get_const_view() const noexcept -> DirectDamageEventsConstView;

    FrameArray<FRegistryEntityHandle> damaged_entities;
    FrameArray<std::int32_t> damage_amounts;
    FrameArray<FRegistryEntityHandle> instigators;
};
} // namespace ml::simulation::lasers
