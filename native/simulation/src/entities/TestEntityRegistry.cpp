#include "sandbox/simulation/entities/TestEntityRegistry.h"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sandbox/simulation/direct_damage_events.h>
#include <sandbox/simulation/entities/NativeEntityRegistryView.h>
#include <sandbox/simulation/entity_combat_accounting.h>
#include <sandbox/simulation/entity_death_accounting.h>
#include <sandbox/simulation/entity_death_info.h>
#include <sandbox/simulation/entity_registry_history.h>
#include <sandbox/simulation/entity_registry_refresh.h>
#include <sandbox/simulation/entity_registry_spawn.h>

#include <algorithm>
#include <cassert>
#include <format>
#include <sandbox/core/diagnostics.h>
#include <utility>

namespace entity_registry_detail {
template <typename Error>
void check_accounting_result(std::expected<void, Error> const& result) {
    if (result) {
        return;
    }

    switch (result.error().code) {
        case ml::simulation::UniqueIdLookupError::InvalidHandle:
            assert(false);
            return;
        case ml::simulation::UniqueIdLookupError::MissingStaleHandle:
            assert(false && "A missing unique ID should be impossible here.");
            return;
    }
}
} // namespace entity_registry_detail

/* **************************************** */
// Lifecycle
/* **************************************** */
void FTestEntityRegistry::reset() {
    entity_data.reset();
    queued_entity_data.reset();
    unique_entity_history_.reset();
    queued_death_infos.reset();
    queued_direct_damage_events.reset();
    bookkeeping_.reset();
    statistics_.reset();
}
void FTestEntityRegistry::begin_tick() {
    bookkeeping_.begin_tick();
}
void FTestEntityRegistry::commit_updates() {

    validate_unique_queued_entity_update_handles();
    commit_entity_updates();
    commit_death_updates();

    queued_entity_data.reset();
    queued_death_infos.reset();
    bookkeeping_.clear_queued_updates();

    validate_array_sizes();
}
void FTestEntityRegistry::refresh_free_indices() {

    bookkeeping_.refresh_free_indices(
        std::span{entity_data.alive.data(), static_cast<std::size_t>(entity_data.alive.size())});
}
void FTestEntityRegistry::end_tick() {

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
auto FTestEntityRegistry::add_entities(EntityData::ConstView const view) -> SpawnedEntityHandles {

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
            ml::simulation::register_spawned_entity(bookkeeping_,
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
        auto const handle{
            ml::simulation::register_spawned_entity(bookkeeping_,
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
void FTestEntityRegistry::queue_entity_updates(ConstView const view,
                                               EntityDeathInfo const& death_info) {

    assert(view.indices.size() == static_cast<std::size_t>(view.data.num()));
    queued_entity_data.append_from(view.data);
    bookkeeping_.queue_update_handles(
        std::span{view.indices.data(), static_cast<std::size_t>(view.indices.size())});

    death_info.validate_array_sizes();
    queued_death_infos.append_from(death_info.get_const_view());
}
void FTestEntityRegistry::commit_entity_updates() {

    [[maybe_unused]] auto const count{queued_entity_data.num()};
    assert(static_cast<std::int32_t>(bookkeeping_.queued_update_handles.size()) == count);
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    [[maybe_unused]] auto const invalid_update{ml::simulation::apply_entity_updates(
        bookkeeping_,
        statistics_,
        unique_entities,
        ml::make_native_update_view(entity_data.get_view()),
        ml::make_native_update_view(queued_entity_data.get_const_view()))};
    assert(invalid_update < 0);
}
void FTestEntityRegistry::commit_death_updates() {

    queued_death_infos.validate_array_sizes();
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    auto const result{ml::simulation::record_entity_deaths(
        bookkeeping_, statistics_, unique_entities, queued_death_infos.get_const_view())};
    entity_registry_detail::check_accounting_result(result);
}

/* **************************************** */
// Damage events
/* **************************************** */
void FTestEntityRegistry::queue_direct_damage_events(
    DirectDamageEventsConstView const damage_events) {
    damage_events.validate_array_sizes();

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ml::simulation::record_damage_events(statistics_,
                                                           bookkeeping_.generations,
                                                           bookkeeping_.unique_ids,
                                                           unique_entities,
                                                           damage_events)};
    entity_registry_detail::check_accounting_result(result);

    queued_direct_damage_events.append_from(damage_events);
}
void FTestEntityRegistry::record_shots(std::span<FRegistryEntityHandle const> const instigators) {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ml::simulation::record_shots(
        statistics_,
        bookkeeping_.generations,
        bookkeeping_.unique_ids,
        unique_entities,
        {instigators.data(), static_cast<std::size_t>(instigators.size())})};
    entity_registry_detail::check_accounting_result(result);
}
auto FTestEntityRegistry::get_direct_damage_queue_view() const -> DirectDamageEvents const& {
    return queued_direct_damage_events;
}

/* **************************************** */
// Handle queries
/* **************************************** */
auto FTestEntityRegistry::analyse_handle(FRegistryEntityHandle const handle) const
    -> ml::simulation::RegistryHandleState {
    return bookkeeping_.analyse_handle(handle);
}
auto FTestEntityRegistry::is_stale(FRegistryEntityHandle const handle) const -> bool {
    return bookkeeping_.is_stale(handle);
}

/* **************************************** */
// Entity data updates
/* **************************************** */
void FTestEntityRegistry::refresh_handles(std::span<FRegistryEntityHandle> const handles) const {
    [[maybe_unused]] auto const invalid_index{ml::simulation::refresh_registry_handles(
        ml::make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())})};
    assert(invalid_index < 0);
}
void FTestEntityRegistry::refresh_locations(std::span<FRegistryEntityHandle const> handles,
                                            ml::simulation::Vectors3fView const& locations) {
    [[maybe_unused]] auto const n{handles.size()};
    assert(static_cast<std::size_t>(locations.num()) == n);

    [[maybe_unused]] auto const inactive_index{ml::simulation::copy_registry_entity_data(
        ml::make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())},
        {.locations = locations, .velocities = {}})};
    assert(inactive_index < 0);
}
void FTestEntityRegistry::refresh_entity_data(std::span<FRegistryEntityHandle> handles,
                                              ml::simulation::Vectors3fView const& locations,
                                              ml::simulation::Vectors3fView const& velocities) {
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
    [[maybe_unused]] auto const inactive_index{ml::simulation::copy_registry_entity_data(
        ml::make_native_query_view(*this),
        {handles.data(), static_cast<std::size_t>(handles.size())},
        {.locations = update_locations ? locations : ml::simulation::Vectors3fView{},
         .velocities = update_velocities ? velocities : ml::simulation::Vectors3fView{}})};
    assert(inactive_index < 0);
}

/* **************************************** */
// Entity data queries
/* **************************************** */
auto FTestEntityRegistry::get_location(FRegistryEntityHandle const handle) const
    -> ml::simulation::Vector3f {
    assert(is_valid_handle(handle));
    return entity_data.locations[handle.index];
}
auto FTestEntityRegistry::get_velocity(FRegistryEntityHandle const handle) const
    -> ml::simulation::Vector3f {
    assert(is_valid_handle(handle));
    return entity_data.velocities[handle.index];
}
auto FTestEntityRegistry::get_health(FRegistryEntityHandle const handle) const -> std::int32_t {
    assert(is_valid_handle(handle));
    return entity_data.healths[handle.index];
}
auto FTestEntityRegistry::get_team(FRegistryEntityHandle const handle) const
    -> ml::simulation::Team {
    assert(is_valid_handle(handle));
    return entity_data.teams[handle.index];
}
auto FTestEntityRegistry::get_entity_type(FRegistryEntityHandle const handle) const
    -> ml::simulation::EntityType {
    assert(is_valid_handle(handle));
    return entity_data.entity_types[handle.index];
}
auto FTestEntityRegistry::get_alive(FRegistryEntityHandle const handle) const -> bool {
    assert(is_valid_handle(handle));
    return static_cast<bool>(entity_data.alive[handle.index]);
}

/* **************************************** */
// Entity collection queries
/* **************************************** */
auto FTestEntityRegistry::get_moved_entities_this_tick() const
    -> std::span<FRegistryEntityHandle const> {
    return {bookkeeping_.moved_entities.data(), bookkeeping_.moved_entities.size()};
}
auto FTestEntityRegistry::get_dead_entities_this_frame() const
    -> std::span<FRegistryEntityHandle const> {
    return {bookkeeping_.dead_entities.data(), bookkeeping_.dead_entities.size()};
}
auto FTestEntityRegistry::get_handles_not_in_team(ml::simulation::Team const team) const
    -> std::vector<FRegistryEntityHandle> {
    std::vector<FRegistryEntityHandle> out;
    get_handles_not_in_team(team, out);
    return out;
}
void FTestEntityRegistry::get_handles_not_in_team(ml::simulation::Team const team,
                                                  std::vector<FRegistryEntityHandle>& out) const {
    out.resize(static_cast<std::size_t>(get_num_elements()));
    auto const count{ml::simulation::collect_non_team_alive_entities(
        ml::make_native_query_view(*this),
        team,
        {out.data(), static_cast<std::size_t>(out.size())})};
    out.resize(static_cast<std::size_t>(count));
}

/* **************************************** */
// Aggregate queries
/* **************************************** */
auto FTestEntityRegistry::get_num_elements() const noexcept -> std::int32_t {
    return entity_data.num();
}
auto FTestEntityRegistry::get_num_alive_active_entities() const noexcept -> std::int32_t {
    return statistics_.alive_count();
}
auto FTestEntityRegistry::count_kills() const noexcept -> std::int32_t {
    return statistics_.cumulative_kill_count();
}
auto FTestEntityRegistry::count_alive() const noexcept -> std::int32_t {
    return statistics_.alive_count();
}
auto FTestEntityRegistry::count_alive(ml::simulation::EntityType const type) const noexcept
    -> std::int32_t {
    return statistics_.count_alive(type);
}
auto FTestEntityRegistry::count_alive_per_team() const noexcept -> TeamCounts {
    return statistics_.count_alive_per_team();
}
auto FTestEntityRegistry::count_alive_per_team_and_type() const noexcept -> EntityCounts {
    return statistics_.count_alive_per_team_and_type();
}
auto FTestEntityRegistry::count_alive_not_on_team(ml::simulation::Team const team) const noexcept
    -> std::int32_t {
    return statistics_.count_alive_not_on_team(team);
}

/* **************************************** */
// Unique entity queries
/* **************************************** */
auto FTestEntityRegistry::is_valid_unique_id(ml::simulation::EntityUniqueId const id) const
    -> bool {
    return ml::simulation::is_valid_unique_id(id, get_num_unique_ids_issued());
}
auto FTestEntityRegistry::find_unique_id(FRegistryEntityHandle const handle) const
    -> ml::simulation::EntityUniqueId {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const result{ml::simulation::find_entity_unique_id(
        bookkeeping_.generations, bookkeeping_.unique_ids, unique_entities, handle)};
    if (result) {
        return *result;
    }

    switch (result.error()) {
        case ml::simulation::UniqueIdLookupError::InvalidHandle:
            assert(false);
            return {};
        case ml::simulation::UniqueIdLookupError::MissingStaleHandle:
            assert(false && "A missing unique ID should be impossible here.");
            return {};
    }
    return {};
}
auto FTestEntityRegistry::get_kills(ml::simulation::EntityUniqueId const id) const
    -> std::uint32_t {
    assert(is_valid_unique_id(id));
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    return unique_entities.kills[id.id];
}

/* **************************************** */
// Spatial queries
/* **************************************** */
auto FTestEntityRegistry::collect_entities_in_range(
    ml::simulation::Vector3f const& origin,
    float const radius,
    std::span<FRegistryEntityHandle> const out_entities) const -> std::int32_t {

    return ml::simulation::collect_entities_in_range(
        ml::make_native_query_view(*this),
        origin,
        radius,
        {out_entities.data(), static_cast<std::size_t>(out_entities.size())});
}

/* **************************************** */
// Validation
/* **************************************** */
void FTestEntityRegistry::validate_unique_queued_entity_update_handles() const {
#ifndef NDEBUG
    std::vector<bool> seen_handles(static_cast<std::size_t>(entity_data.num()), false);
    for (auto const handle : bookkeeping_.queued_update_handles) {
        assert(is_valid_handle(handle));
        assert(!seen_handles[handle.index] && "Entity update handle queued more than once");
        seen_handles[handle.index] = true;
    }
#endif
}
void FTestEntityRegistry::validate_array_sizes() const {
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
void FTestEntityRegistry::validate_unique_ids() const {

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
void FTestEntityRegistry::validate_unique_entity_data() const {

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n{unique_entities.num()};

    for (std::int32_t i{0}; i < n; ++i) {
        FRegistryEntityHandle const handle{
            unique_entities.registry_indices[i],
            unique_entities.registry_generations[i],
        };
        [[maybe_unused]] auto const handle_status{analyse_handle(handle)};
        assert(handle_status != ml::simulation::RegistryHandleState::Invalid);
        assert(handle_status != ml::simulation::RegistryHandleState::Null);

        assert(unique_entities.life_state[i] == ml::simulation::LifeState::Alive ||
               unique_entities.life_state[i] == ml::simulation::LifeState::Unknown ||
               unique_entities.life_state[i] == ml::simulation::LifeState::Combat);
    }
}
void FTestEntityRegistry::validate_handles(std::span<FRegistryEntityHandle const> const handles) {
    for (auto const handle : handles) {
        if (!is_valid_handle(handle)) {
            ml::fatal_error("Entity handle is invalid");
        }
    }
}
