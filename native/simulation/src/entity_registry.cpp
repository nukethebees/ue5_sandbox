#include "ioj/sim/entity_registry.h"
#include <cstdint>
#include <ioj/sim/profiling.h>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_registry_refresh.h>
#include <ioj/sim/entity_registry_view.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <expected>
#include <format>
#include <sandbox/core/diagnostics.h>
#include <utility>

namespace ioj::sim {

namespace entity_registry_detail {
enum class UniqueIdLookupError : std::uint8_t { InvalidHandle, MissingStaleHandle };

auto find_unique_id(std::span<std::int32_t const> const generations,
                    std::span<EntityUniqueId const> const current_ids,
                    EntityHistoryColumnsConstView const history,
                    RegistryEntityHandle const handle) noexcept
    -> std::expected<EntityUniqueId, UniqueIdLookupError> {
    assert(generations.size() == current_ids.size());
    switch (analyse_handle(generations, handle)) {
        case RegistryHandleState::Active:
            return current_ids[static_cast<std::size_t>(handle.index)];
        case RegistryHandleState::Stale:
            break;
        case RegistryHandleState::Invalid:
        case RegistryHandleState::Null:
            return std::unexpected{UniqueIdLookupError::InvalidHandle};
    }

    auto const count{history.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (history.registry_indices[element] == handle.index &&
            history.registry_generations[element] == handle.generation) {
            return EntityUniqueId{.id = index};
        }
    }
    return std::unexpected{UniqueIdLookupError::MissingStaleHandle};
}

struct AccountingError {
    UniqueIdLookupError code;
    RegistryEntityHandle handle;
    std::int32_t event_index;
};

template <typename Error>
void check_accounting_result(std::expected<void, Error> const& result) {
    if (result) {
        return;
    }

    switch (result.error().code) {
        case UniqueIdLookupError::InvalidHandle:
            assert(false);
            return;
        case UniqueIdLookupError::MissingStaleHandle:
            assert(false && "A missing unique ID should be impossible here.");
            return;
    }
}

auto register_spawned_entity(EntityRegistryBookkeeping& bookkeeping,
                             EntityRegistryStatistics& statistics,
                             EntityHistoryColumnsView const history,
                             std::int32_t const slot_index,
                             EntityUniqueId const unique_id,
                             Team const team,
                             EntityType const type,
                             std::int32_t const health) noexcept -> RegistryEntityHandle {
    assert(slot_index >= 0);
    assert(static_cast<std::size_t>(slot_index) < bookkeeping.generations.size());
    assert(unique_id.id >= 0 && unique_id.id < history.num());

    auto const slot{static_cast<std::size_t>(slot_index)};
    auto const unique{static_cast<std::size_t>(unique_id.id)};
    auto const generation{bookkeeping.generations[slot]};
    bookkeeping.unique_ids[slot] = unique_id;
    history.registry_indices[unique] = slot_index;
    history.registry_generations[unique] = generation;
    auto const alive{is_alive(health)};
    history.life_state[unique] = alive ? LifeState::Alive : LifeState::Unknown;
    history.entity_types[unique] = type;
    history.teams[unique] = team;
    statistics.record_spawn(team, type, alive);
    return {slot_index, generation};
}

auto apply_entity_updates(EntityRegistryBookkeeping& bookkeeping,
                          EntityRegistryStatistics& statistics,
                          EntityHistoryColumnsView const history,
                          RegistryEntityData::View const entities,
                          RegistryEntityData::ConstView const updates) noexcept -> std::int32_t {
    auto const count{updates.num()};
    assert(static_cast<std::int32_t>(bookkeeping.queued_update_handles.size()) == count);
    for (std::int32_t update_index{}; update_index < count; ++update_index) {
        auto const update_element{static_cast<std::size_t>(update_index)};
        auto const handle{bookkeeping.queued_update_handles[update_element]};
        if (!bookkeeping.is_valid_handle(handle)) {
            return update_index;
        }

        auto const slot_index{handle.index};
        auto const slot{static_cast<std::size_t>(slot_index)};
        auto const position_changed{
            entities.locations.xs[slot] != updates.locations.xs[update_element] ||
            entities.locations.ys[slot] != updates.locations.ys[update_element] ||
            entities.locations.zs[slot] != updates.locations.zs[update_element]};
        auto const rotation_changed{
            entities.rotations.pitches[slot] != updates.rotations.pitches[update_element] ||
            entities.rotations.yaws[slot] != updates.rotations.yaws[update_element] ||
            entities.rotations.rolls[slot] != updates.rotations.rolls[update_element]};
        if (position_changed || rotation_changed) {
            bookkeeping.record_moved(handle);
        }

        auto const old_alive{is_alive(entities.healths[slot])};
        auto const new_alive{is_alive(updates.healths[update_element])};
        auto const old_team{entities.teams[slot_index]};
        auto const new_team{updates.teams[update_index]};
        auto const entity_type{entities.entity_types[slot_index]};
        statistics.apply_alive_transition(old_team, new_team, entity_type, old_alive, new_alive);

        entities.teams[slot_index] = new_team;
        auto const unique{static_cast<std::size_t>(bookkeeping.unique_ids[slot].id)};
        if (new_alive) {
            history.life_state[unique] = LifeState::Alive;
        } else if (old_alive) {
            history.life_state[unique] = LifeState::Unknown;
        }
        if (old_team != new_team) {
            history.teams[unique] = new_team;
        }

        entities.locations.set(slot_index, updates.locations[update_index]);
        entities.velocities.set(slot_index, updates.velocities[update_index]);
        entities.rotations.set(slot_index, updates.rotations[update_index]);
        entities.healths[slot] = updates.healths[update_element];
    }
    return -1;
}

auto record_deaths(EntityRegistryBookkeeping& bookkeeping,
                   EntityRegistryStatistics& statistics,
                   EntityHistoryColumnsView const history,
                   EntityDeathInfoConstView const events) noexcept
    -> std::expected<void, AccountingError> {
    auto const count{events.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const victim{events.victims[element]};
        auto const victim_id{find_unique_id(
            bookkeeping.generations, bookkeeping.unique_ids, history.get_const_view(), victim)};
        if (!victim_id) {
            return std::unexpected{AccountingError{victim_id.error(), victim, index}};
        }

        bookkeeping.record_dead(victim);
        auto const victim_element{static_cast<std::size_t>(victim_id->id)};
        history.life_state[victim_element] = static_cast<LifeState>(events.reasons[element]);
        statistics.record_destroyed(history.teams[victim_element],
                                    history.entity_types[victim_element]);

        auto const killer{events.killers[element]};
        if (!killer.is_valid()) {
            continue;
        }
        auto const killer_id{find_unique_id(
            bookkeeping.generations, bookkeeping.unique_ids, history.get_const_view(), killer)};
        if (!killer_id) {
            return std::unexpected{AccountingError{killer_id.error(), killer, index}};
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

auto record_damage(EntityRegistryStatistics& statistics,
                   std::span<std::int32_t const> const generations,
                   std::span<EntityUniqueId const> const current_ids,
                   EntityHistoryColumnsConstView const history,
                   DirectDamageEventsConstView const events) noexcept
    -> std::expected<void, AccountingError> {
    auto const count{events.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const victim{events.damaged_entities[element]};
        auto const victim_id{find_unique_id(generations, current_ids, history, victim)};
        if (!victim_id) {
            return std::unexpected{AccountingError{victim_id.error(), victim, index}};
        }

        auto const victim_element{static_cast<std::size_t>(victim_id->id)};
        auto const damage{static_cast<double>(events.damage_amounts[element])};
        statistics.record_damage_received(
            history.teams[victim_element], history.entity_types[victim_element], damage);

        auto const instigator{events.instigators[element]};
        if (!instigator.is_valid()) {
            continue;
        }
        auto const attacker_id{find_unique_id(generations, current_ids, history, instigator)};
        if (!attacker_id) {
            return std::unexpected{AccountingError{attacker_id.error(), instigator, index}};
        }
        auto const attacker_element{static_cast<std::size_t>(attacker_id->id)};
        statistics.record_hit(
            history.teams[attacker_element], history.entity_types[attacker_element], damage);
    }
    return {};
}

auto record_shots(EntityRegistryStatistics& statistics,
                  std::span<std::int32_t const> const generations,
                  std::span<EntityUniqueId const> const current_ids,
                  EntityHistoryColumnsConstView const history,
                  std::span<RegistryEntityHandle const> const instigators) noexcept
    -> std::expected<void, AccountingError> {
    auto const count{static_cast<std::int32_t>(instigators.size())};
    for (std::int32_t index{}; index < count; ++index) {
        auto const instigator{instigators[static_cast<std::size_t>(index)]};
        if (!instigator.is_valid()) {
            continue;
        }
        auto const attacker_id{find_unique_id(generations, current_ids, history, instigator)};
        if (!attacker_id) {
            return std::unexpected{AccountingError{attacker_id.error(), instigator, index}};
        }
        auto const attacker_element{static_cast<std::size_t>(attacker_id->id)};
        statistics.record_shot(history.teams[attacker_element],
                               history.entity_types[attacker_element]);
    }
    return {};
}

auto copy_entity_data(EntityRegistryQueryView const registry,
                      std::span<RegistryEntityHandle const> const handles,
                      Vectors3fView const locations,
                      Vectors3fView const velocities) noexcept -> std::int32_t {
    auto const count{static_cast<std::int32_t>(handles.size())};
    assert(locations.num() == 0 || locations.num() == count);
    assert(velocities.num() == 0 || velocities.num() == count);
    std::int32_t first_inactive{-1};
    for (std::int32_t index{}; index < count; ++index) {
        auto const handle{handles[static_cast<std::size_t>(index)]};
        if (handle.is_null()) {
            if (!locations.is_empty()) {
                locations.set(index, ml::make_vector3f(0.f, 0.f, 0.f));
            }
            if (!velocities.is_empty()) {
                velocities.set(index, ml::make_vector3f(0.f, 0.f, 0.f));
            }
            continue;
        }
        if (analyse_handle(registry, handle) != RegistryHandleState::Active) {
            if (first_inactive < 0) {
                first_inactive = index;
            }
            continue;
        }
        if (!locations.is_empty()) {
            locations.set(index, registry.locations[handle.index]);
        }
        if (!velocities.is_empty()) {
            velocities.set(index, registry.velocities[handle.index]);
        }
    }
    return first_inactive;
}
} // namespace entity_registry_detail

/* **************************************** */
// Lifecycle
/* **************************************** */
void EntityRegistry::reset() {
    entity_data.reset();
    queued_entity_data.reset();
    unique_entity_history_.reset();
    queued_death_infos.reset();
    queued_direct_damage_events.reset();
    bookkeeping_.reset();
    statistics_.reset();
}
void EntityRegistry::begin_tick() {
    bookkeeping_.begin_tick();
}
void EntityRegistry::commit_updates() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::commit_updates");

    validate_unique_queued_entity_update_handles();
    commit_entity_updates();
    commit_death_updates();

    queued_entity_data.reset();
    queued_death_infos.reset();
    bookkeeping_.clear_queued_updates();

    validate_array_sizes();
}
void EntityRegistry::refresh_free_indices() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::refresh_free_indices");

    bookkeeping_.refresh_free_indices(std::span{
        entity_data.healths.data(), static_cast<std::size_t>(entity_data.healths.size())});
}
void EntityRegistry::end_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::end_tick");

    refresh_free_indices();

    queued_direct_damage_events.reset();
    bookkeeping_.clear_dead_entities();

    validate_array_sizes();
    validate_unique_ids();
    validate_unique_entity_data();
}

/* **************************************** */
// Entity creation
/* **************************************** */
auto EntityRegistry::add_entities(EntityData::ConstView const view) -> SpawnedEntityHandles {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::add_entities");

    view.validate_array_sizes();

    auto const count{view.num()};
    SpawnedEntityHandles new_entities;

    if (count < 1) {
        return new_entities;
    }

    new_entities.first_id = {.id = unique_entity_history_.num()};
    unique_entity_history_.add_defaulted(count);
    new_entities.registry_handles.add_uninitialised(count);
    auto const unique_entities{unique_entity_history_.get_view().columns()};

    auto const reuse_count{std::min(bookkeeping_.available_free_slot_count(), count)};
    for (std::int32_t source_index{}; source_index < reuse_count; ++source_index) {
        auto const slot_index{bookkeeping_.take_free_slot()};
        entity_data.copy_element(slot_index, view, source_index);

        auto const handle{
            entity_registry_detail::register_spawned_entity(bookkeeping_,
                                                            statistics_,
                                                            unique_entities,
                                                            slot_index,
                                                            new_entities.first_id + source_index,
                                                            view.teams[source_index],
                                                            view.entity_types[source_index],
                                                            view.healths[source_index])};
        new_entities.registry_handles.set(source_index, handle.index, handle.generation);
    }

    auto const append_count{count - reuse_count};
    auto const first_slot_index{entity_data.num()};
    bookkeeping_.append_slots(append_count);
    entity_data.append_from(view.get_view(reuse_count, append_count));

    for (std::int32_t offset{}; offset < append_count; ++offset) {
        auto const source_index{reuse_count + offset};
        auto const handle{
            entity_registry_detail::register_spawned_entity(bookkeeping_,
                                                            statistics_,
                                                            unique_entities,
                                                            first_slot_index + offset,
                                                            new_entities.first_id + source_index,
                                                            view.teams[source_index],
                                                            view.entity_types[source_index],
                                                            view.healths[source_index])};
        new_entities.registry_handles.set(source_index, handle.index, handle.generation);
    }

    validate_array_sizes();
    validate_unique_ids();
    return new_entities;
}

/* **************************************** */
// Queued updates
/* **************************************** */
void EntityRegistry::queue_entity_updates(ConstView const view, EntityDeathInfo const& death_info) {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::queue_entity_updates");

    assert(view.indices.size() == static_cast<std::size_t>(view.data.num()));
    queued_entity_data.append_from(view.data);
    bookkeeping_.queue_update_handles(
        std::span{view.indices.data(), static_cast<std::size_t>(view.indices.size())});

    death_info.validate_array_sizes();
    queued_death_infos.append_from(death_info.get_const_view());
}
void EntityRegistry::commit_entity_updates() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::commit_entity_updates");

    [[maybe_unused]] auto const count{queued_entity_data.num()};
    assert(static_cast<std::int32_t>(bookkeeping_.queued_update_handles.size()) == count);
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    [[maybe_unused]] auto const invalid_update{
        entity_registry_detail::apply_entity_updates(bookkeeping_,
                                                     statistics_,
                                                     unique_entities,
                                                     entity_data.get_view(),
                                                     queued_entity_data.get_const_view())};
    assert(invalid_update < 0);
}
void EntityRegistry::commit_death_updates() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::commit_death_updates");

    queued_death_infos.validate_array_sizes();
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    auto const result{entity_registry_detail::record_deaths(
        bookkeeping_, statistics_, unique_entities, queued_death_infos.get_const_view())};
    entity_registry_detail::check_accounting_result(result);
}

/* **************************************** */
// Damage events
/* **************************************** */
void EntityRegistry::queue_direct_damage_events(DirectDamageEventsConstView const damage_events) {
    damage_events.validate_array_sizes();

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{entity_registry_detail::record_damage(statistics_,
                                                            bookkeeping_.generations,
                                                            bookkeeping_.unique_ids,
                                                            unique_entities,
                                                            damage_events)};
    entity_registry_detail::check_accounting_result(result);

    queued_direct_damage_events.append_from(damage_events);
}
void EntityRegistry::record_shots(std::span<RegistryEntityHandle const> const instigators) {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{entity_registry_detail::record_shots(statistics_,
                                                           bookkeeping_.generations,
                                                           bookkeeping_.unique_ids,
                                                           unique_entities,
                                                           instigators)};
    entity_registry_detail::check_accounting_result(result);
}
auto EntityRegistry::get_direct_damage_queue_view() const -> DirectDamageEvents const& {
    return queued_direct_damage_events;
}

/* **************************************** */
// Handle queries
/* **************************************** */
auto EntityRegistry::analyse_handle(RegistryEntityHandle const handle) const
    -> RegistryHandleState {
    return bookkeeping_.analyse_handle(handle);
}
auto EntityRegistry::is_stale(RegistryEntityHandle const handle) const -> bool {
    return bookkeeping_.is_stale(handle);
}

/* **************************************** */
// Entity data updates
/* **************************************** */
void EntityRegistry::refresh_handles(std::span<RegistryEntityHandle> const handles) const {
    [[maybe_unused]] auto const invalid_index{refresh_registry_handles(
        make_native_query_view(*this), {handles.data(), static_cast<std::size_t>(handles.size())})};
    assert(invalid_index < 0);
}
void EntityRegistry::refresh_locations(std::span<RegistryEntityHandle const> handles,
                                       Vectors3fView const& locations) {
    [[maybe_unused]] auto const n{handles.size()};
    assert(static_cast<std::size_t>(locations.num()) == n);

    [[maybe_unused]] auto const inactive_index{entity_registry_detail::copy_entity_data(
        make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())},
        locations,
        {})};
    assert(inactive_index < 0);
}
void EntityRegistry::refresh_entity_data(std::span<RegistryEntityHandle> handles,
                                         Vectors3fView const& locations,
                                         Vectors3fView const& velocities) {
    auto const n_handles{handles.size()};
    if (n_handles == 0) {
        return;
    }

    auto should_update_view{[n_handles](std::size_t const n_view) -> bool {
        auto const should_update{n_view == n_handles};
        if (n_view != 0 && !should_update) {
            ml::fatal_error(std::format(
                "Entity data view has {} elements but got {} handles", n_view, n_handles));
        }
        return should_update;
    }};

    refresh_handles(handles);

    auto const update_locations{should_update_view(static_cast<std::size_t>(locations.num()))};
    auto const update_velocities{should_update_view(static_cast<std::size_t>(velocities.num()))};
    [[maybe_unused]] auto const inactive_index{entity_registry_detail::copy_entity_data(
        make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())},
        update_locations ? locations : Vectors3fView{},
        update_velocities ? velocities : Vectors3fView{})};
    assert(inactive_index < 0);
}

/* **************************************** */
// Entity data queries
/* **************************************** */
auto EntityRegistry::get_location(RegistryEntityHandle const handle) const -> Vector3f {
    assert(is_valid_handle(handle));
    return entity_data.locations[handle.index];
}
auto EntityRegistry::get_velocity(RegistryEntityHandle const handle) const -> Vector3f {
    assert(is_valid_handle(handle));
    return entity_data.velocities[handle.index];
}
auto EntityRegistry::get_health(RegistryEntityHandle const handle) const -> std::int32_t {
    assert(is_valid_handle(handle));
    return entity_data.healths[handle.index];
}
auto EntityRegistry::get_team(RegistryEntityHandle const handle) const -> Team {
    assert(is_valid_handle(handle));
    return entity_data.teams[handle.index];
}
auto EntityRegistry::get_entity_type(RegistryEntityHandle const handle) const -> EntityType {
    assert(is_valid_handle(handle));
    return entity_data.entity_types[handle.index];
}
auto EntityRegistry::get_alive(RegistryEntityHandle const handle) const -> bool {
    assert(is_valid_handle(handle));
    return is_alive(entity_data.healths[handle.index]);
}

/* **************************************** */
// Entity collection queries
/* **************************************** */
auto EntityRegistry::get_moved_entities_this_tick() const -> std::span<RegistryEntityHandle const> {
    return {bookkeeping_.moved_entities.data(), bookkeeping_.moved_entities.size()};
}
auto EntityRegistry::get_dead_entities_this_frame() const -> std::span<RegistryEntityHandle const> {
    return {bookkeeping_.dead_entities.data(), bookkeeping_.dead_entities.size()};
}
auto EntityRegistry::get_handles_not_in_team(Team const team) const
    -> std::vector<RegistryEntityHandle> {
    std::vector<RegistryEntityHandle> out;
    get_handles_not_in_team(team, out);
    return out;
}
void EntityRegistry::get_handles_not_in_team(Team const team,
                                             std::vector<RegistryEntityHandle>& out) const {
    out.resize(static_cast<std::size_t>(get_num_elements()));
    auto const count{collect_non_team_alive_entities(
        make_native_query_view(*this), team, {out.data(), static_cast<std::size_t>(out.size())})};
    out.resize(static_cast<std::size_t>(count));
}

/* **************************************** */
// Aggregate queries
/* **************************************** */
auto EntityRegistry::get_num_elements() const noexcept -> std::int32_t {
    return entity_data.num();
}
auto EntityRegistry::get_num_alive_active_entities() const noexcept -> std::int32_t {
    return statistics_.alive_count();
}
auto EntityRegistry::count_kills() const noexcept -> std::int32_t {
    return statistics_.cumulative_kill_count();
}
auto EntityRegistry::count_alive() const noexcept -> std::int32_t {
    return statistics_.alive_count();
}
auto EntityRegistry::count_alive(EntityType const type) const noexcept -> std::int32_t {
    return statistics_.count_alive(type);
}
auto EntityRegistry::count_alive_per_team() const noexcept -> TeamCounts {
    return statistics_.count_alive_per_team();
}
auto EntityRegistry::count_alive_per_team_and_type() const noexcept -> EntityCounts {
    return statistics_.count_alive_per_team_and_type();
}
auto EntityRegistry::count_alive_not_on_team(Team const team) const noexcept -> std::int32_t {
    return statistics_.count_alive_not_on_team(team);
}

/* **************************************** */
// Unique entity queries
/* **************************************** */
auto EntityRegistry::is_valid_unique_id(EntityUniqueId const id) const -> bool {
    return id.id >= 0 && id.id < get_num_unique_ids_issued();
}
auto EntityRegistry::find_unique_id(RegistryEntityHandle const handle) const -> EntityUniqueId {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{entity_registry_detail::find_unique_id(
        bookkeeping_.generations, bookkeeping_.unique_ids, unique_entities, handle)};
    if (result) {
        return *result;
    }

    switch (result.error()) {
        case entity_registry_detail::UniqueIdLookupError::InvalidHandle:
            assert(false);
            return {};
        case entity_registry_detail::UniqueIdLookupError::MissingStaleHandle:
            assert(false && "A missing unique ID should be impossible here.");
            return {};
    }
    return {};
}
auto EntityRegistry::get_kills(EntityUniqueId const id) const -> std::uint32_t {
    assert(is_valid_unique_id(id));
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    return unique_entities.kills[id.id];
}

/* **************************************** */
// Spatial queries
/* **************************************** */
auto EntityRegistry::collect_entities_in_range(
    Vector3f const& origin,
    float const radius,
    std::span<RegistryEntityHandle> const out_entities) const -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::collect_entities_in_range");

    return sim::collect_entities_in_range(
        make_native_query_view(*this),
        origin,
        radius,
        {out_entities.data(), static_cast<std::size_t>(out_entities.size())});
}

/* **************************************** */
// Validation
/* **************************************** */
void EntityRegistry::validate_unique_queued_entity_update_handles() const {
#ifndef NDEBUG
    std::vector<bool> seen_handles(static_cast<std::size_t>(entity_data.num()), false);
    for (auto const handle : bookkeeping_.queued_update_handles) {
        assert(is_valid_handle(handle));
        assert(!seen_handles[handle.index] && "Entity update handle queued more than once");
        seen_handles[handle.index] = true;
    }
#endif
}
void EntityRegistry::validate_array_sizes() const {
    auto const entity_count{entity_data.num()};
    if (bookkeeping_.generations.size() != static_cast<std::size_t>(entity_count) ||
        bookkeeping_.unique_ids.size() != static_cast<std::size_t>(entity_count)) {
        ml::fatal_error("Entity registry column counts differ");
    }

    entity_data.validate_array_sizes();
    queued_direct_damage_events.validate_array_sizes();

#ifndef NDEBUG
    queued_entity_data.validate_array_sizes();
    unique_entity_history_.get_const_view().columns().validate_array_sizes();
    assert(queued_entity_data.num() ==
           static_cast<std::int32_t>(bookkeeping_.queued_update_handles.size()));
#endif
}
void EntityRegistry::validate_unique_ids() const {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::validate_unique_ids");

    // Development-time validation for the unique id system
    [[maybe_unused]] auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n{static_cast<std::int32_t>(bookkeeping_.unique_ids.size())};

    for (std::int32_t i{0}; i < n; ++i) {
        auto const unique_id{bookkeeping_.unique_ids[i]};
        if (!is_valid_unique_id(unique_id)) {
            ml::fatal_error(std::format("Invalid unique entity ID: id[{}] = {}", i, unique_id.id));
        }
        assert(unique_entities.registry_indices[unique_id.id] == i);
        assert(unique_entities.registry_generations[unique_id.id] == bookkeeping_.generations[i]);
    }
}
void EntityRegistry::validate_unique_entity_data() const {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::validate_unique_entity_data");

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n{unique_entities.num()};

    for (std::int32_t i{0}; i < n; ++i) {
        RegistryEntityHandle const handle{
            unique_entities.registry_indices[i],
            unique_entities.registry_generations[i],
        };
        [[maybe_unused]] auto const handle_status{analyse_handle(handle)};
        assert(handle_status != RegistryHandleState::Invalid);
        assert(handle_status != RegistryHandleState::Null);

        assert(unique_entities.life_state[i] == LifeState::Alive ||
               unique_entities.life_state[i] == LifeState::Unknown ||
               unique_entities.life_state[i] == LifeState::Combat);
    }
}
void EntityRegistry::validate_handles(std::span<RegistryEntityHandle const> const handles) {
    for (auto const handle : handles) {
        if (!is_valid_handle(handle)) {
            ml::fatal_error("Entity handle is invalid");
        }
    }
}
} // namespace ioj::sim
