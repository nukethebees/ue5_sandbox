#include "sandbox/simulation/batch_damage.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>

namespace ml::simulation {
auto sort_and_deduplicate_removal_indices(std::span<std::int32_t> const indices) noexcept
    -> std::int32_t {
    std::ranges::sort(indices, std::greater{});
    auto const unique_end{std::ranges::unique(indices).begin()};
    return static_cast<std::int32_t>(unique_end - indices.begin());
}

auto resolve_batch_damage(std::span<FRegistryEntityHandle const> const entity_handles,
                          std::span<std::int32_t> const healths,
                          DirectDamageEventsConstView const damage_events,
                          std::span<std::int32_t> const removal_indices,
                          std::int32_t removal_count,
                          EntityDeathInfoView const deaths,
                          std::int32_t death_count) noexcept -> BatchDamageResult {
    assert(healths.size() == entity_handles.size());
    assert(removal_count >= 0 && static_cast<std::size_t>(removal_count) <= removal_indices.size());
    assert(death_count >= 0 && death_count <= deaths.num());

    auto const event_count{damage_events.num()};
    assert(removal_indices.size() - static_cast<std::size_t>(removal_count) >=
           static_cast<std::size_t>(event_count));
    assert(deaths.num() - death_count >= event_count);

    for (std::int32_t event_index{}; event_index < event_count; ++event_index) {
        auto const event_element{static_cast<std::size_t>(event_index)};
        auto const damaged_handle{damage_events.damaged_entities[event_element]};
        auto const entity{std::ranges::find(entity_handles, damaged_handle)};
        if (entity == entity_handles.end()) {
            continue;
        }

        auto const local_index{static_cast<std::int32_t>(entity - entity_handles.begin())};
        auto const local_element{static_cast<std::size_t>(local_index)};
        healths[local_element] -= damage_events.damage_amounts[event_element];
        auto const removals{removal_indices.first(static_cast<std::size_t>(removal_count))};
        if (healths[local_element] > 0 ||
            std::ranges::find(removals, local_index) != removals.end()) {
            continue;
        }

        removal_indices[static_cast<std::size_t>(removal_count++)] = local_index;
        auto const instigator{damage_events.instigators[event_element]};
        auto const reason{instigator.is_null() ? DeathReason::Unknown : DeathReason::Combat};
        deaths.set(death_count++, reason, entity_handles[local_element], instigator);
    }
    return {removal_count, death_count};
}
} // namespace ml::simulation
