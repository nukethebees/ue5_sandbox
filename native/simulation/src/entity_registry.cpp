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

void copy_entity_rows(RegistryEntityDataView const destination,
                      std::int32_t const destination_offset,
                      RegistryEntityDataConstView const source,
                      std::int32_t const source_offset,
                      std::int32_t const count) {
    if (count == 0) {
        return;
    }

    auto copy_span = [destination_offset, source_offset, count](auto const source_column,
                                                                auto const destination_column) {
        std::ranges::copy(source_column.subspan(source_offset, count),
                          destination_column.subspan(destination_offset, count).begin());
    };
    copy_span(source.locations.xs_span(), destination.locations.xs_span());
    copy_span(source.locations.ys_span(), destination.locations.ys_span());
    copy_span(source.locations.zs_span(), destination.locations.zs_span());
    copy_span(source.velocities.xs_span(), destination.velocities.xs_span());
    copy_span(source.velocities.ys_span(), destination.velocities.ys_span());
    copy_span(source.velocities.zs_span(), destination.velocities.zs_span());
    copy_span(source.rotations.pitches, destination.rotations.pitches);
    copy_span(source.rotations.yaws, destination.rotations.yaws);
    copy_span(source.rotations.rolls, destination.rotations.rolls);
    copy_span(source.healths, destination.healths);
    copy_span(source.teams, destination.teams);
    copy_span(source.entity_types, destination.entity_types);
}

void append_entity_rows(SingleAllocationRegistryEntityData& destination,
                        RegistryEntityDataConstView const source) {
    auto const destination_offset{destination.num()};
    destination.add_uninitialised(source.num());
    copy_entity_rows(destination.get_view().columns(), destination_offset, source, 0, source.num());
}

auto apply_entity_updates(EntityRegistryBookkeeping& bookkeeping,
                          EntityLedger& ledger,
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

        ledger.record_status(bookkeeping.unique_ids[slot],
                             updates.teams[update_index],
                             is_alive(updates.healths[update_element]));
        entities.teams[slot_index] = updates.teams[update_index];

        entities.locations.set(slot_index, updates.locations[update_index]);
        entities.velocities.set(slot_index, updates.velocities[update_index]);
        entities.rotations.set(slot_index, updates.rotations[update_index]);
        entities.healths[slot] = updates.healths[update_element];
    }
    return -1;
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
    ledger_.reset();
    historical_handles_.clear();
    queued_death_infos.reset();
    combat_events_.reset();
    bookkeeping_.reset();
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

    bookkeeping_.refresh_free_indices(entity_data.get_const_view().healths());
}
void EntityRegistry::end_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::end_tick");

    refresh_free_indices();

    combat_events_.reset();
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

    new_entities.entity_ids.reserve(static_cast<std::size_t>(count));
    new_entities.registry_handles.add_uninitialised(count);
    auto const reuse_count{std::min(bookkeeping_.available_free_slot_count(), count)};
    auto const first_slot_index{entity_data.num()};
    bookkeeping_.append_slots(count - reuse_count);
    entity_registry_detail::append_entity_rows(entity_data,
                                               view.get_view(reuse_count, count - reuse_count));
    for (std::int32_t i{}; i < count; ++i) {
        auto const slot{i < reuse_count ? bookkeeping_.take_free_slot()
                                        : first_slot_index + i - reuse_count};
        if (i < reuse_count) {
            entity_registry_detail::copy_entity_rows(
                entity_data.get_view().columns(), slot, view, i, 1);
        }
        auto const id{
            ledger_.record_spawn(view.entity_types[i], view.teams[i], is_alive(view.healths[i]))};
        RegistryEntityHandle const handle{slot, bookkeeping_.generations[slot]};
        bookkeeping_.unique_ids[slot] = id;
        historical_handles_.push_back(handle);
        new_entities.entity_ids.push_back(id);
        new_entities.registry_handles.set(i, handle.index, handle.generation);
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
    entity_registry_detail::append_entity_rows(queued_entity_data, view.data);
    bookkeeping_.queue_update_handles(
        std::span{view.indices.data(), static_cast<std::size_t>(view.indices.size())});

    death_info.validate_array_sizes();
    queued_death_infos.append_from(death_info.get_const_view());
}
void EntityRegistry::commit_entity_updates() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::commit_entity_updates");

    [[maybe_unused]] auto const count{queued_entity_data.num()};
    assert(static_cast<std::int32_t>(bookkeeping_.queued_update_handles.size()) == count);
    [[maybe_unused]] auto const invalid_update{entity_registry_detail::apply_entity_updates(
        bookkeeping_,
        ledger_,
        entity_data.get_view().columns(),
        queued_entity_data.get_const_view().columns())};
    assert(invalid_update < 0);
}
void EntityRegistry::commit_death_updates() {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::commit_death_updates");

    queued_death_infos.validate_array_sizes();
    auto const events{queued_death_infos.get_const_view()};
    auto const count{events.num()};
    for (std::int32_t i{}; i < count; ++i) {
        auto const victim{events.victims[i]};
        auto const row{ledger_.get_history_index(victim)};
        assert(row >= 0);
        bookkeeping_.record_dead(historical_handles_[row]);
        ledger_.record_death(victim, events.killers[i], events.reasons[i]);
    }
}

/* **************************************** */
// Damage events
/* **************************************** */
void EntityRegistry::queue_direct_damage_events(DirectDamageEventsConstView const damage_events) {
    combat_events_.queue_damage(damage_events);
}
void EntityRegistry::record_shots(std::span<EntityUniqueId const> const instigators) {
    ledger_.record_shots(instigators);
}
auto EntityRegistry::get_direct_damage_queue_view() const -> DirectDamageEvents const& {
    return combat_events_.all_events();
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
    return entity_data.get_const_view().columns().locations[handle.index];
}
auto EntityRegistry::get_velocity(RegistryEntityHandle const handle) const -> Vector3f {
    assert(is_valid_handle(handle));
    return entity_data.get_const_view().columns().velocities[handle.index];
}
auto EntityRegistry::get_health(RegistryEntityHandle const handle) const -> Health {
    assert(is_valid_handle(handle));
    return entity_data.get_const_view().healths()[handle.index];
}
auto EntityRegistry::get_team(RegistryEntityHandle const handle) const -> Team {
    assert(is_valid_handle(handle));
    return entity_data.get_const_view().teams()[handle.index];
}
auto EntityRegistry::get_entity_type(RegistryEntityHandle const handle) const -> EntityType {
    assert(is_valid_handle(handle));
    return entity_data.get_const_view().entity_types()[handle.index];
}
auto EntityRegistry::get_alive(RegistryEntityHandle const handle) const -> bool {
    assert(is_valid_handle(handle));
    return is_alive(entity_data.get_const_view().healths()[handle.index]);
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
    return ledger_.count_alive();
}
auto EntityRegistry::count_kills() const noexcept -> std::int32_t {
    return ledger_.count_kills();
}
auto EntityRegistry::count_alive() const noexcept -> std::int32_t {
    return ledger_.count_alive();
}
auto EntityRegistry::count_alive(EntityType const type) const noexcept -> std::int32_t {
    return ledger_.count_alive(type);
}
auto EntityRegistry::count_alive_per_team() const noexcept -> TeamCounts {
    return ledger_.count_alive_per_team();
}
auto EntityRegistry::count_alive_per_team_and_type() const noexcept -> EntityCounts {
    return ledger_.count_alive_per_team_and_type();
}
auto EntityRegistry::count_alive_not_on_team(Team const team) const noexcept -> std::int32_t {
    return ledger_.count_alive_not_on_team(team);
}

/* **************************************** */
// Unique entity queries
/* **************************************** */
auto EntityRegistry::is_valid_unique_id(EntityUniqueId const id) const -> bool {
    return get_history_index(id) >= 0;
}
auto EntityRegistry::find_unique_id(RegistryEntityHandle const handle) const -> EntityUniqueId {
    if (is_valid_handle(handle)) {
        return bookkeeping_.unique_ids[handle.index];
    }
    auto const found{std::ranges::find(historical_handles_, handle)};
    assert(found != historical_handles_.end());
    return ledger_.get_unique_entities().entity_ids[found - historical_handles_.begin()];
}
auto EntityRegistry::get_kills(EntityUniqueId const id) const -> std::uint32_t {
    return ledger_.get_kills(id);
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

    entity_data.get_const_view().columns().validate_array_sizes();
    combat_events_.all_events().validate_array_sizes();

#ifndef NDEBUG
    queued_entity_data.get_const_view().columns().validate_array_sizes();
    ledger_.get_unique_entities().validate_array_sizes();
    assert(queued_entity_data.num() ==
           static_cast<std::int32_t>(bookkeeping_.queued_update_handles.size()));
#endif
}
void EntityRegistry::validate_unique_ids() const {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::validate_unique_ids");

    // Development-time validation for the unique id system
    [[maybe_unused]] auto const unique_entities{ledger_.get_unique_entities()};
    auto const n{static_cast<std::int32_t>(bookkeeping_.unique_ids.size())};

    for (std::int32_t i{0}; i < n; ++i) {
        auto const unique_id{bookkeeping_.unique_ids[i]};
        if (!is_valid_unique_id(unique_id)) {
            ml::fatal_error(
                std::format("Invalid unique entity ID: id[{}] = {}", i, unique_id.raw_value()));
        }
        [[maybe_unused]] auto const index{get_history_index(unique_id)};
        assert(historical_handles_[index] ==
               (RegistryEntityHandle{i, bookkeeping_.generations[i]}));
        assert(unique_entities.entity_types[index] == unique_id.entity_type());
    }
}
void EntityRegistry::validate_unique_entity_data() const {
    SANDBOX_PROFILE_SCOPE("Sandbox::EntityRegistry::validate_unique_entity_data");

    auto const unique_entities{ledger_.get_unique_entities()};
    auto const n{unique_entities.num()};

    for (std::int32_t i{0}; i < n; ++i) {
        auto const handle{historical_handles_[i]};
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
