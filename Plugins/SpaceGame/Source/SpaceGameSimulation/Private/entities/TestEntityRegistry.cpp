#include "SpaceGameSimulation/entities/TestEntityRegistry.h"

#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <utility>

/* **************************************** */
// Lifecycle
/* **************************************** */
void FTestEntityRegistry::reset() {
    ml::reset(entity_data,
              queued_entity_data,
              unique_entity_history_,
              queued_death_infos,
              queued_direct_damage_events);
    bookkeeping_.reset();
    statistics_.reset();
}
void FTestEntityRegistry::begin_tick() {
    bookkeeping_.begin_tick();
}
void FTestEntityRegistry::commit_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_updates);

    validate_unique_queued_entity_update_handles();
    commit_entity_updates();
    commit_death_updates();

    ml::reset(queued_entity_data, queued_death_infos);
    bookkeeping_.clear_queued_updates();

    validate_array_sizes();
}
void FTestEntityRegistry::refresh_free_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::refresh_free_indices);

    bookkeeping_.refresh_free_indices(
        std::span{entity_data.alive.GetData(), static_cast<std::size_t>(entity_data.alive.Num())});
}
void FTestEntityRegistry::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::end_tick);

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
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::add_entities);

    view.validate_array_sizes();

    auto const count{view.num()};
    SpawnedEntityHandles new_entities;

    if (count < 1) {
        return new_entities;
    }

    new_entities.first_id = {.id = unique_entity_history_.num()};
    unique_entity_history_.add_defaulted(count);
    new_entities.registry_handles.add_uninitialised(count);

    auto const reuse_count{FMath::Min(bookkeeping_.available_free_slot_count(), count)};
    for (int32 source_index{}; source_index < reuse_count; ++source_index) {
        auto const slot_index{bookkeeping_.take_free_slot()};
        entity_data.copy_element(slot_index, view, source_index);

        auto const handle{register_spawned_entity(
            view, source_index, slot_index, new_entities.first_id + source_index)};
        new_entities.registry_handles.set(source_index, handle.index, handle.generation);
    }

    auto const append_count{count - reuse_count};
    auto const first_slot_index{entity_data.num()};
    bookkeeping_.append_slots(append_count);
    entity_data.append_from(view.get_view(reuse_count, append_count));

    for (int32 offset{}; offset < append_count; ++offset) {
        auto const source_index{reuse_count + offset};
        auto const handle{register_spawned_entity(
            view, source_index, first_slot_index + offset, new_entities.first_id + source_index)};
        new_entities.registry_handles.set(source_index, handle.index, handle.generation);
    }

    validate_array_sizes();
    validate_unique_ids();
    return new_entities;
}
FORCEINLINE auto FTestEntityRegistry::register_spawned_entity(EntityData::ConstView const& view,
                                                              int32 const source_index,
                                                              int32 const slot_index,
                                                              TestEntityUniqueId const unique_id)
    -> FRegistryEntityHandle {
    auto const generation{bookkeeping_.generations[slot_index]};
    auto const team{view.teams[source_index]};
    auto const type{view.entity_types[source_index]};
    auto const alive{view.alive[source_index]};
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    bookkeeping_.unique_ids[slot_index] = unique_id;
    unique_entities.registry_indices[unique_id.id] = slot_index;
    unique_entities.registry_generations[unique_id.id] = generation;
    unique_entities.alive[unique_id.id] = alive;
    unique_entities.entity_types[unique_id.id] = ml::to_native(type);
    unique_entities.teams[unique_id.id] = ml::to_native(team);

    statistics_.record_spawn(ml::to_native(team), ml::to_native(type), alive != 0);
    return {slot_index, generation};
}

/* **************************************** */
// Queued updates
/* **************************************** */
void FTestEntityRegistry::queue_entity_updates(ConstView const view,
                                               EntityDeathInfo const& death_info) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::queue_entity_updates);

    check(view.indices.Num() == view.data.num());
    queued_entity_data.append_from(view.data);
    bookkeeping_.queue_update_handles(
        std::span{view.indices.GetData(), static_cast<std::size_t>(view.indices.Num())});

    death_info.validate_array_sizes();
    queued_death_infos.append_from(death_info.get_const_view());
}
void FTestEntityRegistry::commit_entity_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_entity_updates);

    auto const count{queued_entity_data.num()};
    check(static_cast<int32>(bookkeeping_.queued_update_handles.size()) == count);

    for (int32 update_index{}; update_index < count; ++update_index) {
        auto const handle{bookkeeping_.queued_update_handles[update_index]};
        check(is_valid_handle(handle));
        auto const slot_index{handle.index};

        auto const position_changed{
            entity_data.locations.xs[slot_index] != queued_entity_data.locations.xs[update_index] ||
            entity_data.locations.ys[slot_index] != queued_entity_data.locations.ys[update_index] ||
            entity_data.locations.zs[slot_index] != queued_entity_data.locations.zs[update_index]};
        auto const rotation_changed{entity_data.rotations.pitches[slot_index] !=
                                        queued_entity_data.rotations.pitches[update_index] ||
                                    entity_data.rotations.yaws[slot_index] !=
                                        queued_entity_data.rotations.yaws[update_index] ||
                                    entity_data.rotations.rolls[slot_index] !=
                                        queued_entity_data.rotations.rolls[update_index]};

        if (position_changed || rotation_changed) {
            bookkeeping_.record_moved(handle);
        }

        apply_live_state_transition(slot_index,
                                    queued_entity_data.teams[update_index],
                                    queued_entity_data.alive[update_index]);
        ml::assign_from(
            entity_data.locations, slot_index, queued_entity_data.locations, update_index);
        ml::assign_from(
            entity_data.velocities, slot_index, queued_entity_data.velocities, update_index);
        ml::assign(entity_data.rotations,
                   slot_index,
                   ml::get_rotator3d(queued_entity_data.rotations, update_index));
        entity_data.healths[slot_index] = queued_entity_data.healths[update_index];
    }
}
void FTestEntityRegistry::commit_death_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_death_updates);

    queued_death_infos.validate_array_sizes();

    auto const n_deaths{queued_death_infos.num()};
    for (int32 i{0}; i < n_deaths; ++i) {
        auto const victim_handle{queued_death_infos.victims[i]};
        auto const victim_id{find_unique_id(victim_handle)};

        bookkeeping_.record_dead(victim_handle);

        record_entity_death(victim_id, queued_death_infos.reasons[i]);

        auto const killer_handle{queued_death_infos.killers[i]};

        if (killer_handle.is_valid()) {
            auto const killer_id{find_unique_id(killer_handle)};
            credit_entity_kill(killer_id, victim_id);
        }
    }
}
FORCEINLINE void FTestEntityRegistry::record_entity_death(TestEntityUniqueId const victim_id,
                                                          ETestDeathReason const reason) {
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    unique_entities.alive[victim_id.id] = 0;
    unique_entities.death_reason[victim_id.id] = reason;
    statistics_.record_destroyed(unique_entities.teams[victim_id.id],
                                 unique_entities.entity_types[victim_id.id]);
}
FORCEINLINE void FTestEntityRegistry::credit_entity_kill(TestEntityUniqueId const killer_id,
                                                         TestEntityUniqueId const victim_id) {
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    unique_entities.killed_by[victim_id.id] = killer_id;
    ++unique_entities.kills[killer_id.id];
    statistics_.record_kill(unique_entities.teams[killer_id.id],
                            unique_entities.entity_types[killer_id.id],
                            unique_entities.teams[victim_id.id]);
}

/* **************************************** */
// Live state and alive counts
/* **************************************** */
FORCEINLINE void FTestEntityRegistry::apply_live_state_transition(int32 const slot_index,
                                                                  ETestTeam const team,
                                                                  uint8 const alive) {
    auto const old_alive{entity_data.alive[slot_index] != 0};
    auto const new_alive{alive != 0};
    auto const old_team{entity_data.teams[slot_index]};
    auto const type{entity_data.entity_types[slot_index]};
    statistics_.apply_alive_transition(
        ml::to_native(old_team), ml::to_native(team), ml::to_native(type), old_alive, new_alive);

    entity_data.teams[slot_index] = team;
    entity_data.alive[slot_index] = alive;
    auto const unique_id{bookkeeping_.unique_ids[slot_index]};
    auto const unique_entities{unique_entity_history_.get_view().columns()};
    unique_entities.alive[unique_id.id] = alive;
    if (old_team != team) {
        unique_entities.teams[unique_id.id] = ml::to_native(team);
    }
}

/* **************************************** */
// Damage events
/* **************************************** */
void FTestEntityRegistry::queue_direct_damage_events(
    DirectDamageEventsConstView const damage_events) {
    damage_events.validate_array_sizes();

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const count{damage_events.num()};
    for (int32 index{}; index < count; ++index) {
        auto const victim_id{find_unique_id(damage_events.damaged_entities[index])};
        auto const victim_team{unique_entities.teams[victim_id.id]};
        auto const victim_type{unique_entities.entity_types[victim_id.id]};
        auto const damage{static_cast<double>(damage_events.damage_amounts[index])};
        statistics_.record_damage_received(victim_team, victim_type, damage);

        auto const instigator{damage_events.instigators[index]};
        if (instigator.is_valid()) {
            auto const attacker_id{find_unique_id(instigator)};
            auto const attacker_team{unique_entities.teams[attacker_id.id]};
            auto const attacker_type{unique_entities.entity_types[attacker_id.id]};
            statistics_.record_hit(attacker_team, attacker_type, damage);
        }
    }

    queued_direct_damage_events.append_from(damage_events);
}
void FTestEntityRegistry::record_shots(TConstArrayView<FRegistryEntityHandle> const instigators) {
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    for (auto const instigator : instigators) {
        if (!instigator.is_valid()) {
            continue;
        }
        auto const attacker_id{find_unique_id(instigator)};
        auto const team{unique_entities.teams[attacker_id.id]};
        auto const type{unique_entities.entity_types[attacker_id.id]};
        statistics_.record_shot(team, type);
    }
}
auto FTestEntityRegistry::get_direct_damage_queue_view() const -> DirectDamageEvents const& {
    return queued_direct_damage_events;
}

/* **************************************** */
// Handle queries
/* **************************************** */
auto FTestEntityRegistry::analyse_handle(FRegistryEntityHandle const handle) const
    -> ERegistryHandleState {
    return bookkeeping_.analyse_handle(handle);
}
auto FTestEntityRegistry::is_stale(FRegistryEntityHandle const handle) const -> bool {
    return bookkeeping_.is_stale(handle);
}

/* **************************************** */
// Entity data updates
/* **************************************** */
void FTestEntityRegistry::refresh_handles(TArrayView<FRegistryEntityHandle> const handles) const {
    for (auto& handle : handles) {
        auto const handle_state{analyse_handle(handle)};

        switch (handle_state) {
            case ERegistryHandleState::Invalid: {
                check(false);
                break;
            }
            case ERegistryHandleState::Null: {
                break;
            }
            case ERegistryHandleState::Stale: {
                handle.reset();
                break;
            }
            case ERegistryHandleState::Active: {
                if (!entity_data.alive[handle.index]) {
                    handle.reset();
                }
                break;
            }
        }
    }
}
void FTestEntityRegistry::refresh_locations(TConstArrayView<FRegistryEntityHandle> handles,
                                            FVectors3f::View const& locations) {
    auto const n{handles.Num()};
    check(ml::num(locations) == n);

    for (int32 i{}; i < n; ++i) {
        auto const handle{handles[i]};
        if (handle.is_null()) {
            locations.set(i, FVector3f::ZeroVector);
        } else {
            check(bookkeeping_.is_valid_handle(handle));

            locations.set(i, entity_data.locations[handle.index]);
        }
    }
}
void FTestEntityRegistry::refresh_entity_data(TArrayView<FRegistryEntityHandle> handles,
                                              FVectors3f::View const& locations,
                                              FVectors3f::View const& velocities,
                                              TArrayView<float> const radii) {
    auto const n_handles{ml::num(handles)};
    if (n_handles == 0) {
        return;
    }

    auto should_update_view{[n_handles](auto const& view) -> bool {
        auto const n_view{ml::num(view)};
        auto const should_update{n_view == n_handles};

        if (!((n_view == 0) || should_update)) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("View has %d elements but got %d handles"),
                   n_view,
                   n_handles);
        }

        return should_update;
    }};

    refresh_handles(handles);

    if (should_update_view(locations)) {
        refresh_locations(handles, locations);
    }

    if (should_update_view(velocities)) {
        for (int32 i{}; i < n_handles; ++i) {
            auto const handle{handles[i]};

            if (handle.is_null()) {
                velocities.set(i, FVector3f::ZeroVector);
                continue;
            }

            ml::assign_from(velocities, i, entity_data.velocities, handle.index);
        }
    }

    if (should_update_view(radii)) {
        for (int32 i{}; i < n_handles; ++i) {
            auto const handle{handles[i]};
            radii[i] = handle.is_null() ? 0.f : entity_data.radii[handle.index];
        }
    }
}

/* **************************************** */
// Entity data queries
/* **************************************** */
auto FTestEntityRegistry::get_location(FRegistryEntityHandle const handle) const -> FVector3f {
    check(is_valid_handle(handle));
    return ml::get_vector3f(entity_data.locations, handle.index);
}
auto FTestEntityRegistry::get_velocity(FRegistryEntityHandle const handle) const -> FVector3f {
    check(is_valid_handle(handle));
    return ml::get_vector3f(entity_data.velocities, handle.index);
}
auto FTestEntityRegistry::get_health(FRegistryEntityHandle const handle) const -> int32 {
    check(is_valid_handle(handle));
    return entity_data.healths[handle.index];
}
auto FTestEntityRegistry::get_team(FRegistryEntityHandle const handle) const -> ETestTeam {
    check(is_valid_handle(handle));
    return entity_data.teams[handle.index];
}
auto FTestEntityRegistry::get_entity_type(FRegistryEntityHandle const handle) const
    -> ETestEntityType {
    check(is_valid_handle(handle));
    return entity_data.entity_types[handle.index];
}
auto FTestEntityRegistry::get_alive(FRegistryEntityHandle const handle) const -> bool {
    check(is_valid_handle(handle));
    return static_cast<bool>(entity_data.alive[handle.index]);
}

/* **************************************** */
// Entity collection queries
/* **************************************** */
auto FTestEntityRegistry::get_moved_entities_this_tick() const
    -> TConstArrayView<FRegistryEntityHandle> {
    return {bookkeeping_.moved_entities.data(),
            static_cast<int32>(bookkeeping_.moved_entities.size())};
}
auto FTestEntityRegistry::get_dead_entities_this_frame() const
    -> TConstArrayView<FRegistryEntityHandle> {
    return {bookkeeping_.dead_entities.data(),
            static_cast<int32>(bookkeeping_.dead_entities.size())};
}
auto FTestEntityRegistry::get_handles_not_in_team(ETestTeam const team) const
    -> TArray<FRegistryEntityHandle> {
    TArray<FRegistryEntityHandle> out;
    get_handles_not_in_team(team, out);
    return out;
}
void FTestEntityRegistry::get_handles_not_in_team(ETestTeam const team,
                                                  TArray<FRegistryEntityHandle>& out) const {
    out.Reset();

    auto const n{get_num_elements()};
    for (int32 i{0}; i < n; ++i) {
        if (entity_data.teams[i] == team) {
            continue;
        }
        if (entity_data.alive[i] <= 0) {
            continue;
        }

        out.Emplace(i, bookkeeping_.generations[i]);
    }
}

/* **************************************** */
// Aggregate queries
/* **************************************** */
auto FTestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return entity_data.num();
}
auto FTestEntityRegistry::get_num_alive_active_entities() const noexcept -> int32 {
    return statistics_.alive_count();
}
auto FTestEntityRegistry::count_kills() const noexcept -> int32 {
    return statistics_.cumulative_kill_count();
}
auto FTestEntityRegistry::count_alive() const noexcept -> int32 {
    return statistics_.alive_count();
}
auto FTestEntityRegistry::count_alive(ETestEntityType const type) const noexcept -> int32 {
    return statistics_.count_alive(ml::to_native(type));
}
auto FTestEntityRegistry::count_alive_per_team() const noexcept -> TeamCounts {
    return statistics_.count_alive_per_team();
}
auto FTestEntityRegistry::count_alive_per_team_and_type() const noexcept -> EntityCounts {
    return statistics_.count_alive_per_team_and_type();
}
auto FTestEntityRegistry::count_alive_not_on_team(ETestTeam const team) const noexcept -> int32 {
    return statistics_.count_alive_not_on_team(ml::to_native(team));
}

/* **************************************** */
// Unique entity queries
/* **************************************** */
auto FTestEntityRegistry::is_valid_unique_id(TestEntityUniqueId const id) const -> bool {
    return id.id >= 0 && id.id < get_num_unique_ids_issued();
}
auto FTestEntityRegistry::find_unique_id(FRegistryEntityHandle const handle) const
    -> TestEntityUniqueId {
    auto const handle_state{analyse_handle(handle)};

    switch (handle_state) {
        case ERegistryHandleState::Null:
            [[fallthrough]];
        case ERegistryHandleState::Invalid: {
            check(false);
            return {};
        }
        case ERegistryHandleState::Active: {
            return bookkeeping_.unique_ids[handle.index];
        }
        case ERegistryHandleState::Stale: {
            break;
        }
        default: {
            check(false);
            return {};
        }
    }

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n_unique{unique_entities.num()};

    for (int32 i{0}; i < n_unique; ++i) {
        if ((unique_entities.registry_indices[i] == handle.index) &&
            (unique_entities.registry_generations[i] == handle.generation)) {
            return {.id = i};
        }
    }

    checkf(false, TEXT("A missing unique ID should be impossible here."));
    return {};
}
auto FTestEntityRegistry::get_kills(TestEntityUniqueId const id) const -> uint32 {
    check(is_valid_unique_id(id));
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    return unique_entities.kills[id.id];
}

/* **************************************** */
// Spatial queries
/* **************************************** */
auto FTestEntityRegistry::collect_entities_in_range(
    FVector3f const& origin,
    float const radius,
    TArrayView<FRegistryEntityHandle> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::collect_entities_in_range);

    int32 count{0};

    auto const radius_squared{radius * radius};
    auto const n{get_num_elements()};
    auto const n_out_limit{out_entities.Num()};

    auto const ox{origin.X};
    auto const oy{origin.Y};
    auto const oz{origin.Z};

    for (int32 i{0}; i < n; ++i) {
        auto const dist_sq{ml::dist_sq(entity_data.locations, i, ox, oy, oz)};

        if (dist_sq <= radius_squared) {
            out_entities[count++] = FRegistryEntityHandle{i, bookkeeping_.generations[i]};
        }

        if (count >= n_out_limit) {
            break;
        }
    }

    return count;
}

/* **************************************** */
// Validation
/* **************************************** */
void FTestEntityRegistry::validate_unique_queued_entity_update_handles() const {
#if DO_CHECK
    TBitArray<> seen_handles{false, entity_data.num()};
    for (auto const handle : bookkeeping_.queued_update_handles) {
        check(is_valid_handle(handle));
        checkf(!seen_handles[handle.index],
               TEXT("Entity update handle %s was queued more than once in one tick"),
               *LexToString(handle));
        seen_handles[handle.index] = true;
    }
#endif
}
void FTestEntityRegistry::validate_array_sizes() const {
    auto const entity_count{entity_data.num()};
    ml::fatal_if_nums_not_equal({entity_count,
                                 static_cast<int32>(bookkeeping_.generations.size()),
                                 static_cast<int32>(bookkeeping_.unique_ids.size())});

    entity_data.validate_array_sizes();
    queued_direct_damage_events.validate_array_sizes();

#if DO_CHECK
    queued_entity_data.validate_array_sizes();
    unique_entity_history_.get_const_view().columns().validate_array_sizes();
    check(queued_entity_data.num() ==
          static_cast<int32>(bookkeeping_.queued_update_handles.size()));
#endif
}
void FTestEntityRegistry::validate_unique_ids() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::validate_unique_ids);

    // Development-time validation for the unique id system
    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n{static_cast<int32>(bookkeeping_.unique_ids.size())};

    for (int32 i{0}; i < n; ++i) {
        auto const unique_id{bookkeeping_.unique_ids[i]};
        if (!is_valid_unique_id(unique_id)) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Invalid unique id detected: (id[%d] = %d)"),
                   i,
                   unique_id.id);
        }
        check(unique_entities.registry_indices[unique_id.id] == i);
        check(unique_entities.registry_generations[unique_id.id] == bookkeeping_.generations[i]);
    }
}
void FTestEntityRegistry::validate_unique_entity_data() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::validate_unique_entity_data);

    auto const unique_entities{unique_entity_history_.get_const_view().columns()};
    auto const n{unique_entities.num()};

    for (int32 i{0}; i < n; ++i) {
        FRegistryEntityHandle const handle{
            unique_entities.registry_indices[i],
            unique_entities.registry_generations[i],
        };
        auto const handle_status{analyse_handle(handle)};
        check(handle_status != ERegistryHandleState::Invalid);
        check(handle_status != ERegistryHandleState::Null);

        if (unique_entities.alive[i]) {
            continue;
        }
        check(unique_entities.death_reason[i] != ETestDeathReason::Unset);
    }
}
void FTestEntityRegistry::validate_handles(TConstArrayView<FRegistryEntityHandle> const handles) {
    for (auto const handle : handles) {
        if (!is_valid_handle(handle)) {
            UE_LOG(LogSandbox, Fatal, TEXT("Handle is invalid"));
        }
    }
}
