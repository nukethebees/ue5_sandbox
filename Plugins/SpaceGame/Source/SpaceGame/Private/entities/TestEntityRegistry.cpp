#include "SpaceGame/entities/TestEntityRegistry.h"

#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <utility>

/* **************************************** */
// Spawned entity handles
/* **************************************** */
auto SpawnedEntityHandles::num() const -> int32 {
    return registry_handles.num();
}
void SpawnedEntityHandles::reset() {
    registry_handles.reset();
}
void SpawnedEntityHandles::add_defaulted(int32 const count) {
    registry_handles.add_defaulted(count);
}
void SpawnedEntityHandles::add_uninitialised(int32 const count) {
    registry_handles.add_uninitialised(count);
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void FTestEntityRegistry::reset() {
    entity_data.reset();
    queued_entity_data.reset();
    unique_entities.reset();
    queued_death_infos.reset();

    ml::reset(generations,
              unique_ids,
              queued_entity_update_handles,
              queued_direct_damage_events,
              dead_entities_this_frame,
              free_indices);

    alive_counts_ = {};
    alive_count_ = 0;
    cumulative_kill_count_ = 0;
    combat_telemetry_ = {};
}
void FTestEntityRegistry::commit_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_updates);

    commit_entity_updates();
    commit_death_updates();

    validate_array_sizes();
}
void FTestEntityRegistry::refresh_free_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::refresh_free_indices);

    free_indices.Reset();
    auto const n{entity_data.num()};
    for (int32 i{0}; i < n; ++i) {
        if (entity_data.alive[i] == 0u) {
            free_indices.Add(i);
        }
    }
}
void FTestEntityRegistry::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::end_tick);

    refresh_free_indices();

    ml::reset(queued_entity_data,
              queued_entity_update_handles,
              queued_death_infos,
              queued_direct_damage_events,
              dead_entities_this_frame);

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

    new_entities.first_id = {.id = unique_entities.num()};
    unique_entities.add_defaulted(count);
    new_entities.registry_handles.add_uninitialised(count);

    auto const reuse_count{FMath::Min(free_indices.Num(), count)};
    reuse_slots(view, reuse_count, new_entities);
    append_slots(view, reuse_count, new_entities);

    validate_array_sizes();
    validate_unique_ids();

    return new_entities;
}
void FTestEntityRegistry::reuse_slots(EntityData::ConstView const& view,
                                      int32 const count,
                                      SpawnedEntityHandles& spawned) {
    for (int32 source_index{}; source_index < count; ++source_index) {
        auto const slot_index{free_indices.Pop(EAllowShrinking::No)};
        entity_data.copy_element(slot_index, view, source_index);
        ++generations[slot_index];

        auto const handle{register_spawned_entity(
            view, source_index, slot_index, spawned.first_id + source_index)};
        spawned.registry_handles.set(source_index, handle.index, handle.generation);
    }
}
void FTestEntityRegistry::append_slots(EntityData::ConstView const& view,
                                       int32 const source_offset,
                                       SpawnedEntityHandles& spawned) {
    auto const append_count{view.num() - source_offset};
    auto const first_slot_index{entity_data.num()};
    generations.AddZeroed(append_count);
    unique_ids.AddDefaulted(append_count);
    entity_data.append_from(view.get_view(source_offset, append_count));

    for (int32 offset{}; offset < append_count; ++offset) {
        auto const source_index{source_offset + offset};
        auto const handle{register_spawned_entity(
            view, source_index, first_slot_index + offset, spawned.first_id + source_index)};
        spawned.registry_handles.set(source_index, handle.index, handle.generation);
    }
}
FORCEINLINE auto FTestEntityRegistry::register_spawned_entity(EntityData::ConstView const& view,
                                                              int32 const source_index,
                                                              int32 const slot_index,
                                                              TestEntityUniqueId const unique_id)
    -> FRegistryEntityHandle {
    auto const generation{generations[slot_index]};
    auto const team{view.teams[source_index]};
    auto const type{view.entity_types[source_index]};
    auto const alive{view.alive[source_index]};
    unique_ids[slot_index] = unique_id;
    unique_entities.registry_indices[unique_id.id] = slot_index;
    unique_entities.registry_generations[unique_id.id] = generation;
    unique_entities.alive[unique_id.id] = alive;
    unique_entities.entity_types[unique_id.id] = type;
    unique_entities.teams[unique_id.id] = team;

    ++combat_telemetry_.spawned[std::to_underlying(team)][std::to_underlying(type)];
    if (alive != 0) {
        adjust_alive_count(team, type, 1);
    }
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
    queued_entity_update_handles.Append(view.indices);

    death_info.validate_array_sizes();
    queued_death_infos.append_from(death_info);
}
void FTestEntityRegistry::commit_entity_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::commit_entity_updates);

    auto const count{queued_entity_data.num()};
    check(queued_entity_update_handles.Num() == count);

    for (int32 update_index{}; update_index < count; ++update_index) {
        auto const handle{queued_entity_update_handles[update_index]};
        check(is_valid_handle(handle));
        auto const slot_index{handle.index};

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

    dead_entities_this_frame.Reset();
    queued_death_infos.validate_array_sizes();

    auto const n_deaths{queued_death_infos.num()};
    for (int32 i{0}; i < n_deaths; ++i) {
        auto const victim_handle{queued_death_infos.victims[i]};
        auto const victim_id{find_unique_id(victim_handle)};

        dead_entities_this_frame.Add(victim_handle);

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
    unique_entities.alive[victim_id.id] = 0;
    unique_entities.death_reason[victim_id.id] = reason;
    auto const team_index{std::to_underlying(unique_entities.teams[victim_id.id])};
    auto const type_index{std::to_underlying(unique_entities.entity_types[victim_id.id])};
    ++combat_telemetry_.destroyed[team_index][type_index];
    ++combat_telemetry_.losses[team_index][type_index];
}
FORCEINLINE void FTestEntityRegistry::credit_entity_kill(TestEntityUniqueId const killer_id,
                                                         TestEntityUniqueId const victim_id) {
    unique_entities.killed_by[victim_id.id] = killer_id;
    ++unique_entities.kills[killer_id.id];
    ++cumulative_kill_count_;
    auto const killer_team{std::to_underlying(unique_entities.teams[killer_id.id])};
    auto const killer_type{std::to_underlying(unique_entities.entity_types[killer_id.id])};
    auto const victim_team{std::to_underlying(unique_entities.teams[victim_id.id])};
    ++combat_telemetry_.kills[killer_team][killer_type];
    ++combat_telemetry_.kill_matrix[killer_team][victim_team];
}

/* **************************************** */
// Live state and alive counts
/* **************************************** */
void FTestEntityRegistry::adjust_alive_count(ETestTeam const team,
                                             ETestEntityType const type,
                                             int32 const delta) {
    auto const team_index{std::to_underlying(team)};
    auto const type_index{std::to_underlying(type)};
    constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    constexpr auto type_count{ml::EnumCountTrait<ETestEntityType>::count_value};
    check(team_index >= 0 && team_index < team_count);
    check(type_index >= 0 && type_index < type_count);

    auto& count{alive_counts_[team_index][type_index]};
    count += delta;
    alive_count_ += delta;
    check(count >= 0);
    check(alive_count_ >= 0);
}
FORCEINLINE void FTestEntityRegistry::apply_live_state_transition(int32 const slot_index,
                                                                  ETestTeam const team,
                                                                  uint8 const alive) {
    auto const old_alive{entity_data.alive[slot_index] != 0};
    auto const new_alive{alive != 0};
    auto const old_team{entity_data.teams[slot_index]};
    auto const type{entity_data.entity_types[slot_index]};
    if (old_alive && (!new_alive || old_team != team)) {
        adjust_alive_count(old_team, type, -1);
    }
    if (new_alive && (!old_alive || old_team != team)) {
        adjust_alive_count(team, type, 1);
    }

    entity_data.teams[slot_index] = team;
    entity_data.alive[slot_index] = alive;
    auto const unique_id{unique_ids[slot_index]};
    unique_entities.alive[unique_id.id] = alive;
    if (old_team != team) {
        unique_entities.teams[unique_id.id] = team;
    }
}

/* **************************************** */
// Damage events
/* **************************************** */
void FTestEntityRegistry::queue_direct_damage_events(DirectDamageEvents const& damage_events) {
    damage_events.validate_array_sizes();

    auto const count{damage_events.num()};
    for (int32 index{}; index < count; ++index) {
        auto const victim_id{find_unique_id(damage_events.damaged_entities[index])};
        auto const victim_team{unique_entities.teams[victim_id.id]};
        auto const victim_type{unique_entities.entity_types[victim_id.id]};
        auto const damage{static_cast<double>(damage_events.damage_amounts[index])};
        combat_telemetry_
            .damage_received[std::to_underlying(victim_team)][std::to_underlying(victim_type)] +=
            damage;

        auto const instigator{damage_events.instigators[index]};
        if (instigator.is_valid()) {
            auto const attacker_id{find_unique_id(instigator)};
            auto const attacker_team{unique_entities.teams[attacker_id.id]};
            auto const attacker_type{unique_entities.entity_types[attacker_id.id]};
            ++combat_telemetry_
                  .hits[std::to_underlying(attacker_team)][std::to_underlying(attacker_type)];
            combat_telemetry_.damage_dealt[std::to_underlying(attacker_team)]
                                          [std::to_underlying(attacker_type)] += damage;
        }
    }

    queued_direct_damage_events.append_from(damage_events);
}
void FTestEntityRegistry::record_shots(TConstArrayView<FRegistryEntityHandle> const instigators) {
    for (auto const instigator : instigators) {
        if (!instigator.is_valid()) {
            continue;
        }
        auto const attacker_id{find_unique_id(instigator)};
        auto const team{unique_entities.teams[attacker_id.id]};
        auto const type{unique_entities.entity_types[attacker_id.id]};
        ++combat_telemetry_.shots[std::to_underlying(team)][std::to_underlying(type)];
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
    if (handle.is_null()) {
        return ERegistryHandleState::Null;
    }
    if (!generations.IsValidIndex(handle.index)) {
        return ERegistryHandleState::Invalid;
    }
    auto const current_generation{generations[handle.index]};
    if (current_generation == handle.generation) {
        return ERegistryHandleState::Active;
    }
    if (current_generation > handle.generation) {
        return ERegistryHandleState::Stale;
    }

    return ERegistryHandleState::Invalid;
}
auto FTestEntityRegistry::is_stale(FRegistryEntityHandle const handle) const -> bool {
    return generations.IsValidIndex(handle.index) &&
           (generations[handle.index] > handle.generation);
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
            check(generations.IsValidIndex(handle.index));
            check(handle.generation == generations[handle.index]);

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
auto FTestEntityRegistry::get_dead_entities_this_frame() const
    -> TConstArrayView<FRegistryEntityHandle> {
    return dead_entities_this_frame;
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

        out.Emplace(i, generations[i]);
    }
}

/* **************************************** */
// Aggregate queries
/* **************************************** */
auto FTestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return entity_data.num();
}
auto FTestEntityRegistry::get_num_alive_active_entities() const noexcept -> int32 {
    return alive_count_;
}
auto FTestEntityRegistry::count_kills() const noexcept -> int32 {
    return cumulative_kill_count_;
}
auto FTestEntityRegistry::count_alive() const noexcept -> int32 {
    return alive_count_;
}
auto FTestEntityRegistry::count_alive(ETestEntityType const type) const noexcept -> int32 {
    int32 total{0};
    auto const type_index{std::to_underlying(type)};
    for (auto const& team_counts : alive_counts_) {
        total += team_counts[type_index];
    }

    return total;
}
auto FTestEntityRegistry::count_alive_per_team() const noexcept -> TeamCounts {
    TeamCounts out{};

    constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    for (int32 team_index{}; team_index < team_count; ++team_index) {
        for (auto const count : alive_counts_[team_index]) {
            out[team_index] += count;
        }
    }

    return out;
}
auto FTestEntityRegistry::count_alive_per_team_and_type() const noexcept -> EntityCounts {
    return alive_counts_;
}
auto FTestEntityRegistry::count_alive_not_on_team(ETestTeam const team) const noexcept -> int32 {
    int32 count{0};

    constexpr auto team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    for (int32 team_index{}; team_index < team_count; ++team_index) {
        if (team_index == std::to_underlying(team)) {
            continue;
        }
        for (auto const type_count : alive_counts_[team_index]) {
            count += type_count;
        }
    }

    return count;
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
            return unique_ids[handle.index];
        }
        case ERegistryHandleState::Stale: {
            break;
        }
        default: {
            check(false);
            return {};
        }
    }

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
auto FTestEntityRegistry::get_kills(TestEntityUniqueId const id) const
    -> TestEntityUniqueEntityData::kills_type {
    check(is_valid_unique_id(id));
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
            out_entities[count++] = FRegistryEntityHandle{i, generations[i]};
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
void FTestEntityRegistry::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_data),
        SANDBOX_NAMED_NUM(generations),
        SANDBOX_NAMED_NUM(unique_ids),
    });

    entity_data.validate_array_sizes();
    queued_direct_damage_events.validate_array_sizes();

#if DO_CHECK
    queued_entity_data.validate_array_sizes();
    unique_entities.validate_array_sizes();
    check(queued_entity_data.num() == queued_entity_update_handles.Num());
#endif
}
void FTestEntityRegistry::validate_unique_ids() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::validate_unique_ids);

    // Development-time validation for the unique id system
    auto const n{unique_ids.Num()};

    for (int32 i{0}; i < n; ++i) {
        if (!is_valid_unique_id(unique_ids[i])) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Invalid unique id detected: (id[%d] = %d)"),
                   i,
                   unique_ids[i].id);
        }
        check(unique_entities.registry_indices[unique_ids[i].id] == i);
        check(unique_entities.registry_generations[unique_ids[i].id] == generations[i]);
    }
}
void FTestEntityRegistry::validate_unique_entity_data() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestEntityRegistry::validate_unique_entity_data);

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
