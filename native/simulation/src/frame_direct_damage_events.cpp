#include "sandbox/simulation/frame_direct_damage_events.h"

namespace ml::simulation::lasers {
FrameDirectDamageEvents::FrameDirectDamageEvents(std::pmr::memory_resource* const resource)
    : damaged_entities{resource}
    , damage_amounts{resource}
    , instigators{resource} {}

void FrameDirectDamageEvents::reserve(std::int32_t const count) {
    damaged_entities.reserve(count);
    damage_amounts.reserve(count);
    instigators.reserve(count);
}

void FrameDirectDamageEvents::add(FRegistryEntityHandle const damaged_entity,
                                  std::int32_t const damage_amount,
                                  FRegistryEntityHandle const instigator) {
    damaged_entities.add(damaged_entity);
    damage_amounts.add(damage_amount);
    instigators.add(instigator);
}

auto FrameDirectDamageEvents::get_const_view() const noexcept -> DirectDamageEventsConstView {
    return {damaged_entities, damage_amounts, instigators};
}
} // namespace ml::simulation::lasers
