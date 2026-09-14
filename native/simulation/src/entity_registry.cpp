#include "ioj/sim/entity_registry.h"
#include <cstdint>
#include <ioj/sim/profiling.h>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_combat_accounting.h>
#include <ioj/sim/entity_death_accounting.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_registry_history.h>
#include <ioj/sim/entity_registry_refresh.h>
#include <ioj/sim/entity_registry_spawn.h>
#include <ioj/sim/entity_registry_view.h>

#include <algorithm>
#include <cassert>
#include <format>
#include <sandbox/core/diagnostics.h>
#include <utility>

namespace ioj::sim {

namespace entity_registry_detail {
template <typename Error>
void check_accounting_result(std::expected<void, Error> const& result) {
    if (result) {
        return;
    }

    switch (result.error().code) {
        case ioj::sim::UniqueIdLookupError::InvalidHandle:
            assert(false);
            return;
        case ioj::sim::UniqueIdLookupError::MissingStaleHandle:
            assert(false && "A missing unique ID should be impossible here.");
            return;
    }
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

    bookkeeping_.refresh_free_indices(
        std::span{entity_data.alive.data(), static_cast<std::size_t>(entity_data.alive.size())});
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

        auto const handle{ioj::sim::register_spawned_entity(bookkeeping_,
                                                            statistics_,
                                                            unique_entities,
                                                            slot_index,
                                                            new_entities.first_id + source_index,
                                                            view.teams[source_index],
                                                            view.entity_types[source_index],
                                                            view.alive[source_index])};
        new_entities.registry_handles.set(source_index, handle.index, handle.generation);
    }

    auto const append_count{count - reuse_count};
    auto const first_slot_index{entity_data.num()};
    bookkeeping_.append_slots(append_count);
    entity_data.append_from(view.get_view(reuse_count, append_count));

    for (std::int32_t offset{}; offset < append_count; ++offset) {
        auto const source_index{reuse_count + offset};
        auto const handle{ioj::sim::register_spawned_entity(bookkeeping_,
                                                            statistics_,
                                                            unique_entities,
                                                            first_slot_index + offset,
                                                            new_entities.first_id + source_index,
                                                            view.teams[source_index],
                                                            view.entity_types[source_index],
                                                            view.alive[source_index])};
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
    [[maybe_unused]] auto const invalid_update{ioj::sim::apply_entity_updates(
        bookkeeping_,
        statistics_,
        unique_entities,
        ioj::sim::make_native_update_view(entity_data.get_view()),
        ioj::sim::make_native_update_view(queued_entity_data.get_const_view()))};
    assert(invalid_update < 0);
}
void EntityRegistry::commit_death_updates() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::commit_death_updates");

    queued_death_infos.validate_array_sizes();
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    auto const result{ioj::sim::record_entity_deaths(
        bookkeeping_, statistics_, unique_entities, queued_death_infos.get_const_view())};
    entity_registry_detail::check_accounting_result(result);
}

/* **************************************** */
// Damage events
/* **************************************** */
void EntityRegistry::queue_direct_damage_events(DirectDamageEventsConstView const damage_events) {
    damage_events.validate_array_sizes();

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ioj::sim::record_damage_events(statistics_,
                                                     bookkeeping_.generations,
                                                     bookkeeping_.unique_ids,
                                                     unique_entities,
                                                     damage_events)};
    entity_registry_detail::check_accounting_result(result);

    queued_direct_damage_events.append_from(damage_events);
}
void EntityRegistry::record_shots(std::span<RegistryEntityHandle const> const instigators) {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{
        ioj::sim::record_shots(statistics_,
                               bookkeeping_.generations,
                               bookkeeping_.unique_ids,
                               unique_entities,
                               {instigators.data(), static_cast<std::size_t>(instigators.size())})};
    entity_registry_detail::check_accounting_result(result);
}
auto EntityRegistry::get_direct_damage_queue_view() const -> DirectDamageEvents const& {
    return queued_direct_damage_events;
}

/* **************************************** */
// Handle queries
/* **************************************** */
auto EntityRegistry::analyse_handle(RegistryEntityHandle const handle) const
    -> ioj::sim::RegistryHandleState {
    return bookkeeping_.analyse_handle(handle);
}
auto EntityRegistry::is_stale(RegistryEntityHandle const handle) const -> bool {
    return bookkeeping_.is_stale(handle);
}

/* **************************************** */
// Entity data updates
/* **************************************** */
void EntityRegistry::refresh_handles(std::span<RegistryEntityHandle> const handles) const {
    [[maybe_unused]] auto const invalid_index{ioj::sim::refresh_registry_handles(
        ioj::sim::make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())})};
    assert(invalid_index < 0);
}
void EntityRegistry::refresh_locations(std::span<RegistryEntityHandle const> handles,
                                       ioj::sim::Vectors3fView const& locations) {
    [[maybe_unused]] auto const n{handles.size()};
    assert(static_cast<std::size_t>(locations.num()) == n);

    [[maybe_unused]] auto const inactive_index{ioj::sim::copy_registry_entity_data(
        ioj::sim::make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())},
        {.locations = locations, .velocities = {}})};
    assert(inactive_index < 0);
}
void EntityRegistry::refresh_entity_data(std::span<RegistryEntityHandle> handles,
                                         ioj::sim::Vectors3fView const& locations,
                                         ioj::sim::Vectors3fView const& velocities) {
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
    [[maybe_unused]] auto const inactive_index{ioj::sim::copy_registry_entity_data(
        ioj::sim::make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())},
        {.locations = update_locations ? locations : ioj::sim::Vectors3fView{},
         .velocities = update_velocities ? velocities : ioj::sim::Vectors3fView{}})};
    assert(inactive_index < 0);
}

/* **************************************** */
// Entity data queries
/* **************************************** */
auto EntityRegistry::get_location(RegistryEntityHandle const handle) const -> ioj::sim::Vector3f {
    assert(is_valid_handle(handle));
    return entity_data.locations[handle.index];
}
auto EntityRegistry::get_velocity(RegistryEntityHandle const handle) const -> ioj::sim::Vector3f {
    assert(is_valid_handle(handle));
    return entity_data.velocities[handle.index];
}
auto EntityRegistry::get_health(RegistryEntityHandle const handle) const -> std::int32_t {
    assert(is_valid_handle(handle));
    return entity_data.healths[handle.index];
}
auto EntityRegistry::get_team(RegistryEntityHandle const handle) const -> ioj::sim::Team {
    assert(is_valid_handle(handle));
    return entity_data.teams[handle.index];
}
auto EntityRegistry::get_entity_type(RegistryEntityHandle const handle) const
    -> ioj::sim::EntityType {
    assert(is_valid_handle(handle));
    return entity_data.entity_types[handle.index];
}
auto EntityRegistry::get_alive(RegistryEntityHandle const handle) const -> bool {
    assert(is_valid_handle(handle));
    return static_cast<bool>(entity_data.alive[handle.index]);
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
auto EntityRegistry::get_handles_not_in_team(ioj::sim::Team const team) const
    -> std::vector<RegistryEntityHandle> {
    std::vector<RegistryEntityHandle> out;
    get_handles_not_in_team(team, out);
    return out;
}
void EntityRegistry::get_handles_not_in_team(ioj::sim::Team const team,
                                             std::vector<RegistryEntityHandle>& out) const {
    out.resize(static_cast<std::size_t>(get_num_elements()));
    auto const count{ioj::sim::collect_non_team_alive_entities(
        ioj::sim::make_native_query_view(*this),
        team,
        {out.data(), static_cast<std::size_t>(out.size())})};
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
auto EntityRegistry::count_alive(ioj::sim::EntityType const type) const noexcept -> std::int32_t {
    return statistics_.count_alive(type);
}
auto EntityRegistry::count_alive_per_team() const noexcept -> TeamCounts {
    return statistics_.count_alive_per_team();
}
auto EntityRegistry::count_alive_per_team_and_type() const noexcept -> EntityCounts {
    return statistics_.count_alive_per_team_and_type();
}
auto EntityRegistry::count_alive_not_on_team(ioj::sim::Team const team) const noexcept
    -> std::int32_t {
    return statistics_.count_alive_not_on_team(team);
}

/* **************************************** */
// Unique entity queries
/* **************************************** */
auto EntityRegistry::is_valid_unique_id(ioj::sim::EntityUniqueId const id) const -> bool {
    return ioj::sim::is_valid_unique_id(id, get_num_unique_ids_issued());
}
auto EntityRegistry::find_unique_id(RegistryEntityHandle const handle) const
    -> ioj::sim::EntityUniqueId {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ioj::sim::find_entity_unique_id(
        bookkeeping_.generations, bookkeeping_.unique_ids, unique_entities, handle)};
    if (result) {
        return *result;
    }

    switch (result.error()) {
        case ioj::sim::UniqueIdLookupError::InvalidHandle:
            assert(false);
            return {};
        case ioj::sim::UniqueIdLookupError::MissingStaleHandle:
            assert(false && "A missing unique ID should be impossible here.");
            return {};
    }
    return {};
}
auto EntityRegistry::get_kills(ioj::sim::EntityUniqueId const id) const -> std::uint32_t {
    assert(is_valid_unique_id(id));
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    return unique_entities.kills[id.id];
}

/* **************************************** */
// Spatial queries
/* **************************************** */
auto EntityRegistry::collect_entities_in_range(
    ioj::sim::Vector3f const& origin,
    float const radius,
    std::span<RegistryEntityHandle> const out_entities) const -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::collect_entities_in_range");

    return ioj::sim::collect_entities_in_range(
        ioj::sim::make_native_query_view(*this),
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
        assert(handle_status != ioj::sim::RegistryHandleState::Invalid);
        assert(handle_status != ioj::sim::RegistryHandleState::Null);

        assert(unique_entities.life_state[i] == ioj::sim::LifeState::Alive ||
               unique_entities.life_state[i] == ioj::sim::LifeState::Unknown ||
               unique_entities.life_state[i] == ioj::sim::LifeState::Combat);
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
