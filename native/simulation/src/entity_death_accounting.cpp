#include "sandbox/simulation/entity_death_accounting.h"

#include <cstddef>

namespace ml::simulation {
namespace {
auto make_error(UniqueIdLookupError const code,
                FRegistryEntityHandle const handle,
                std::int32_t const event_index) noexcept
    -> std::unexpected<EntityDeathAccountingError> {
    return std::unexpected{EntityDeathAccountingError{code, handle, event_index}};
}
}

auto record_entity_deaths(EntityRegistryBookkeeping& bookkeeping,
                          EntityRegistryStatistics& statistics,
                          EntityHistoryColumnsView const history,
                          EntityDeathInfoConstView const death_events) noexcept
    -> std::expected<void, EntityDeathAccountingError> {
    auto const count{death_events.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const event_element{static_cast<std::size_t>(index)};
        auto const victim_handle{death_events.victims[event_element]};
        auto const victim_id{find_entity_unique_id(bookkeeping.generations,
                                                   bookkeeping.unique_ids,
                                                   history.get_const_view(),
                                                   victim_handle)};
        if (!victim_id) {
            return make_error(victim_id.error(), victim_handle, index);
        }

        bookkeeping.record_dead(victim_handle);

        auto const victim_element{static_cast<std::size_t>(victim_id->id)};
        history.life_state[victim_element] =
            static_cast<LifeState>(death_events.reasons[event_element]);
        statistics.record_destroyed(history.teams[victim_element],
                                    history.entity_types[victim_element]);

        auto const killer_handle{death_events.killers[event_element]};
        if (!killer_handle.is_valid()) {
            continue;
        }

        auto const killer_id{find_entity_unique_id(bookkeeping.generations,
                                                   bookkeeping.unique_ids,
                                                   history.get_const_view(),
                                                   killer_handle)};
        if (!killer_id) {
            return make_error(killer_id.error(), killer_handle, index);
        }

        auto const killer_element{static_cast<std::size_t>(killer_id->id)};
        history.killed_by[victim_element] = *killer_id;
        ++history.kills[killer_element];
        statistics.record_kill(history.teams[killer_element],
                               history.entity_types[killer_element],
                               history.teams[victim_element]);
    }
    return {};
}
} // namespace ml::simulation
