#include "ioj/sim/entity_combat_accounting.h"

#include <cstddef>

namespace ioj::sim {
namespace {
auto make_combat_accounting_error(UniqueIdLookupError const code,
                                  RegistryEntityHandle const handle,
                                  std::int32_t const event_index) noexcept
    -> std::unexpected<EntityCombatAccountingError> {
    return std::unexpected{EntityCombatAccountingError{code, handle, event_index}};
}
}

auto record_damage_events(EntityRegistryStatistics& statistics,
                          std::span<std::int32_t const> const generations,
                          std::span<EntityUniqueId const> const current_unique_ids,
                          EntityHistoryColumnsConstView const history,
                          DirectDamageEventsConstView const damage_events) noexcept
    -> std::expected<void, EntityCombatAccountingError> {
    auto const count{damage_events.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const victim_handle{damage_events.damaged_entities[element]};
        auto const victim_id{
            find_entity_unique_id(generations, current_unique_ids, history, victim_handle)};
        if (!victim_id) {
            return make_combat_accounting_error(victim_id.error(), victim_handle, index);
        }

        auto const victim_element{static_cast<std::size_t>(victim_id->id)};
        auto const damage{static_cast<double>(damage_events.damage_amounts[element])};
        statistics.record_damage_received(
            history.teams[victim_element], history.entity_types[victim_element], damage);

        auto const instigator{damage_events.instigators[element]};
        if (!instigator.is_valid()) {
            continue;
        }
        auto const attacker_id{
            find_entity_unique_id(generations, current_unique_ids, history, instigator)};
        if (!attacker_id) {
            return make_combat_accounting_error(attacker_id.error(), instigator, index);
        }

        auto const attacker_element{static_cast<std::size_t>(attacker_id->id)};
        statistics.record_hit(
            history.teams[attacker_element], history.entity_types[attacker_element], damage);
    }
    return {};
}

auto record_shots(EntityRegistryStatistics& statistics,
                  std::span<std::int32_t const> const generations,
                  std::span<EntityUniqueId const> const current_unique_ids,
                  EntityHistoryColumnsConstView const history,
                  std::span<RegistryEntityHandle const> const instigators) noexcept
    -> std::expected<void, EntityCombatAccountingError> {
    auto const count{static_cast<std::int32_t>(instigators.size())};
    for (std::int32_t index{}; index < count; ++index) {
        auto const instigator{instigators[static_cast<std::size_t>(index)]};
        if (!instigator.is_valid()) {
            continue;
        }

        auto const attacker_id{
            find_entity_unique_id(generations, current_unique_ids, history, instigator)};
        if (!attacker_id) {
            return make_combat_accounting_error(attacker_id.error(), instigator, index);
        }

        auto const attacker_element{static_cast<std::size_t>(attacker_id->id)};
        statistics.record_shot(history.teams[attacker_element],
                               history.entity_types[attacker_element]);
    }
    return {};
}
} // namespace ioj::sim
