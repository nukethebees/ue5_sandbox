#include "ioj/sim/frame_direct_damage_events.h"

namespace ioj::sim::lasers {
FrameDirectDamageEvents::FrameDirectDamageEvents(ml::FrameScratch& scratch)
    : damaged_entities{&scratch}
    , damage_amounts{&scratch}
    , instigators{&scratch} {}

void FrameDirectDamageEvents::reserve(std::int32_t const count) {
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
