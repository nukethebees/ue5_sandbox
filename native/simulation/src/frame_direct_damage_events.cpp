#include "ioj/sim/frame_direct_damage_events.h"

namespace ioj::sim::lasers {
FrameDirectDamageEvents::FrameDirectDamageEvents(std::pmr::memory_resource* const scratch_resource)
    : damaged_entities{scratch_resource}
    , damage_amounts{scratch_resource}
    , instigators{scratch_resource} {}

void FrameDirectDamageEvents::reserve(std::uint32_t const count) {
    damaged_entities.reserve(count);
    damage_amounts.reserve(count);
    instigators.reserve(count);
}

void FrameDirectDamageEvents::add(EntityUniqueId const damaged_entity,
                                  std::int32_t const damage_amount,
                                  EntityUniqueId const instigator) {
    damaged_entities.add(damaged_entity);
    damage_amounts.add(damage_amount);
    instigators.add(instigator);
}

auto FrameDirectDamageEvents::get_const_view() const noexcept -> DirectDamageEventsConstView {
    return {damaged_entities, damage_amounts, instigators};
}
} // namespace lasers
