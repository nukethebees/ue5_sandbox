#include "TestEntityRegistry.h"

#include <Sandbox/batch_game/test_entity_registry/DamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/utilities/actor_utils.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

/* ------------------------------------------------------------------------------------------ */
// NewEntities
/* ------------------------------------------------------------------------------------------ */
auto NewEntities::num() const -> int32 {
    return registry_handles.Num();
}
void NewEntities::reset() {
    ml::reset(registry_handles);
}
void NewEntities::add_defaulted(int32 const count) {
    registry_handles.AddDefaulted(count);
}
void NewEntities::add_uninitialised(int32 const count) {
    registry_handles.AddUninitialized(count);
}

/* ------------------------------------------------------------------------------------------ */
// ATestEntityRegistry
/* ------------------------------------------------------------------------------------------ */
ATestEntityRegistry::ATestEntityRegistry() {
    PrimaryActorTick.bCanEverTick = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestEntityRegistry::reset() {
    entity_data.reset();
    queued_entity_data.reset();

    ml::reset(generations,
              queued_entity_update_handles,
              queued_damage_events,
              dead_entities_this_frame,
              free_indices);
}

// Owners
auto ATestEntityRegistry::register_owner(AActor const& actor) -> TestEntityOwnerId {
    auto const index{entity_owners.Add(&actor)};

    queued_damage_events.AddDefaulted();

    return {static_cast<uint8>(index)};
}
auto ATestEntityRegistry::is_owner(AActor const* const actor) const -> bool {
    return entity_owners.Contains(actor);
}
auto ATestEntityRegistry::is_valid_owner(TestEntityOwnerId const id) const -> bool {
    return entity_owners.IsValidIndex(id.id);
}
auto ATestEntityRegistry::get_owner(AActor const* const actor) -> TestEntityOwnerId {
    auto const index{entity_owners.Find(actor)};
    return index != INDEX_NONE ? TestEntityOwnerId{static_cast<uint8>(index)} : TestEntityOwnerId{};
}

// Entity creation
auto ATestEntityRegistry::add_entities(FTestEntityRegistryEntityData::ConstView const view)
    -> NewEntities {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::add_entities);

    view.validate_array_sizes();

    auto const count{view.num()};

    NewEntities new_entities;
    new_entities.first_id = {
        .id = unique_entities.num(),
    };
    unique_entities.add_defaulted(count);
    auto const first_id{new_entities.first_id};

    auto const n_free_indices{free_indices.Num()};
    auto const free_to_reserve{FMath::Min(n_free_indices, count)};

    int32 new_entity_index{0};

    auto set_up_unique_data{[&](int32 const view_index, int32 const entity_index) {
        auto const unique_id{first_id + new_entity_index};
        unique_ids[entity_index] = unique_id;

        new_entities.registry_handles.Emplace(entity_index, generations[entity_index]);

        auto const i{unique_id.id};

        unique_entities.registry_indices[i] = entity_index;
        unique_entities.registry_generations[i] = generations[entity_index];
        unique_entities.alive[i] = view.alive[view_index];
        unique_entities.entity_types[i] = view.entity_types[view_index];

        new_entity_index++;
    }};

    for (int32 i{0}; i < free_to_reserve; ++i) {
        auto const entity_index{free_indices.Pop(EAllowShrinking::No)};

        ml::assign_from(entity_data.locations, entity_index, view.locations, i);
        ml::assign_from(entity_data.velocities, entity_index, view.velocities, i);

        entity_data.healths[entity_index] = view.healths[i];
        entity_data.teams[entity_index] = view.teams[i];
        entity_data.alive[entity_index] = view.alive[i];
        entity_data.entity_types[entity_index] = view.entity_types[i];

        ++generations[entity_index];

        set_up_unique_data(i, entity_index);
    }

    auto const indices_left_to_reserve{count - new_entities.registry_handles.Num()};
    auto start_index{get_num_elements()};

    generations.AddZeroed(indices_left_to_reserve);
    unique_ids.AddDefaulted(indices_left_to_reserve);

    entity_data.add(view.get_slice(ml::num(new_entities), indices_left_to_reserve));

    for (int32 i{0}; i < indices_left_to_reserve; ++i) {
        auto const entity_index{start_index + i};
        set_up_unique_data(free_to_reserve + i, entity_index);
    }

    validate_array_sizes();
    validate_unique_ids();

    return new_entities;
}

// Damage updates
void ATestEntityRegistry::queue_damage_events(UnresolvedDamageEvents const& damage_events) {
    damage_events.validate_array_sizes();

    auto const n{ml::num(damage_events)};

    for (int32 i{0}; i < n; ++i) {
        auto const id{get_owner(damage_events.damaged_actors[i])};
        if (!id.is_valid()) { continue; }

        auto& actor_damage_events{queued_damage_events[id.id]};
        actor_damage_events.damage_amounts.Add(damage_events.damage_amounts[i]);
        actor_damage_events.actor_components.Add(damage_events.actor_components[i]);
        actor_damage_events.hit_items.Add(damage_events.hit_items[i]);
        actor_damage_events.instigators.Add(damage_events.instigators[i]);
    }
}
auto ATestEntityRegistry::get_damage_queue_view(TestEntityOwnerId const id) const
    -> DamageEvents const& {
    check(is_valid_owner(id));
    return queued_damage_events[id.id];
}

// Entity updates
void ATestEntityRegistry::queue_entity_updates(ConstView const view,
                                               EntityDeathInfo const& death_info) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::queue_entity_updates);

    queued_entity_data.add(view.data);
    queued_entity_update_handles.Append(view.indices);

    death_info.validate_array_sizes();
    queued_death_infos.reasons.Append(death_info.reasons);
    queued_death_infos.victims.Append(death_info.victims);
    queued_death_infos.killers.Append(death_info.killers);
}
void ATestEntityRegistry::commit_entity_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_entity_updates);

    auto const n{queued_entity_data.num()};

    for (int32 i{0}; i < n; ++i) {
        auto const entity_handle{queued_entity_update_handles[i]};
        check(is_valid_handle(entity_handle));

        auto const entity_index{entity_handle.index};

        ml::assign_from(entity_data.locations, entity_index, queued_entity_data.locations, i);
        ml::assign_from(entity_data.velocities, entity_index, queued_entity_data.velocities, i);
        entity_data.healths[entity_index] = queued_entity_data.healths[i];
        entity_data.teams[entity_index] = queued_entity_data.teams[i];
        entity_data.alive[entity_index] = queued_entity_data.alive[i];

        auto const unique_id{unique_ids[entity_index]};
        unique_entities.alive[unique_id.id] = queued_entity_data.alive[i];
    }
}
void ATestEntityRegistry::commit_death_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_death_updates);

    queued_death_infos.validate_array_sizes();
    auto const n_deaths{queued_death_infos.num()};
    for (int32 i{0}; i < n_deaths; ++i) {
        auto const victim_handle{queued_death_infos.victims[i]};
        auto const victim_id{find_unique_id(victim_handle)};

        unique_entities.alive[victim_id.id] = 0;
        unique_entities.death_reason[victim_id.id] = queued_death_infos.reasons[i];

        auto const killer_handle{queued_death_infos.killers[i]};

        if (killer_handle.is_valid()) {
            auto const killer_id{find_unique_id(killer_handle)};
            unique_entities.killed_by[victim_id.id] = killer_id;
            unique_entities.kills[killer_id.id] += 1;
        }
    }

    dead_entities_this_frame.Reset();
    auto const n{entity_data.num()};
    for (int32 i{0}; i < n; ++i) {
        if (entity_data.alive[i] == 0u) { dead_entities_this_frame.Emplace(i, generations[i]); }
    }
}

// Frame events
void ATestEntityRegistry::commit_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_updates);

    commit_entity_updates();
    commit_death_updates();

    validate_array_sizes();
}
void ATestEntityRegistry::refresh_free_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::refresh_free_indices);

    free_indices.Reset();
    auto const n{entity_data.num()};
    for (int32 i{0}; i < n; ++i) {
        if (entity_data.alive[i] == 0u) { free_indices.Add(i); }
    }
}
void ATestEntityRegistry::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::end_tick);

    refresh_free_indices();

    ml::reset(queued_entity_data,
              queued_entity_update_handles,
              queued_entity_update_handles,
              queued_death_infos,
              dead_entities_this_frame);

    auto const n_owners{entity_owners.Num()};
    for (int32 i{0}; i < n_owners; ++i) {
        ml::reset(queued_damage_events[i]);
    }

    validate_array_sizes();
    validate_unique_ids();
    validate_unique_entity_data();
}

// Handle queries
auto ATestEntityRegistry::analyse_handle(FRegistryEntityHandle const handle) const
    -> ERegistryHandleState {
    if (handle.is_null()) { return ERegistryHandleState::Null; }
    if (!generations.IsValidIndex(handle.index)) { return ERegistryHandleState::Invalid; }
    auto const current_generation{generations[handle.index]};
    if (current_generation == handle.generation) { return ERegistryHandleState::Active; }
    if (current_generation > handle.generation) { return ERegistryHandleState::Stale; }

    return ERegistryHandleState::Invalid;
}
auto ATestEntityRegistry::is_stale(FRegistryEntityHandle const index) const -> bool {
    return generations.IsValidIndex(index.index) && (generations[index.index] > index.generation);
}

// Handle updates
void ATestEntityRegistry::refresh_handles(TArrayView<FRegistryEntityHandle> const handles) const {
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
                if (!entity_data.alive[handle.index]) { handle.reset(); }
                break;
            }
        }
    }
}

// Unique id queries
auto ATestEntityRegistry::is_valid_unique_id(TestEntityUniqueId const id) const -> bool {
    return id.is_valid() && (id.id < get_num_unique_ids_issued());
}
auto ATestEntityRegistry::find_unique_id(FRegistryEntityHandle const handle) const
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

auto ATestEntityRegistry::get_kills(TestEntityUniqueId const id) const
    -> TestEntityUniqueEntityData::kills_type {
    check(is_valid_unique_id(id));
    return unique_entities.kills[id.id];
}

// Entity queries
auto ATestEntityRegistry::get_location(FRegistryEntityHandle const index) const -> FVector3f {
    check(is_valid_handle(index));
    return ml::get_vector3f(entity_data.locations, index.index);
}
auto ATestEntityRegistry::get_velocity(FRegistryEntityHandle const index) const -> FVector3f {
    check(is_valid_handle(index));
    return ml::get_vector3f(entity_data.velocities, index.index);
}
auto ATestEntityRegistry::get_health(FRegistryEntityHandle const index) const -> int32 {
    check(is_valid_handle(index));
    return entity_data.healths[index.index];
}
auto ATestEntityRegistry::get_team(FRegistryEntityHandle const index) const -> ETestTeam {
    check(is_valid_handle(index));
    return entity_data.teams[index.index];
}
auto ATestEntityRegistry::get_alive(FRegistryEntityHandle const index) const -> bool {
    check(is_valid_handle(index));
    return static_cast<bool>(entity_data.alive[index.index]);
}
auto ATestEntityRegistry::get_dead_entities_this_frame() const
    -> TConstArrayView<FRegistryEntityHandle> {
    return dead_entities_this_frame;
}

auto ATestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return entity_data.num();
}
auto ATestEntityRegistry::get_num_alive_active_entities() const noexcept -> int32 {
    int32 total{0};

    for (auto const& alive : entity_data.alive) {
        if (alive) { ++total; }
    }

    return total;
}

// Total queries
auto ATestEntityRegistry::count_kills() const noexcept -> int32 {
    int32 n{get_num_unique_ids_issued()};

    int32 total{0};
    for (auto const& kills : unique_entities.kills) {
        total += kills;
    }

    return total;
}
auto ATestEntityRegistry::count_alive() const noexcept -> int32 {
    int32 n{get_num_unique_ids_issued()};

    int32 total{0};
    for (auto const& alive : unique_entities.alive) {
        if (alive) { ++total; }
    }

    return total;
}
auto ATestEntityRegistry::count_alive_not_on_team(ETestTeam const team) const noexcept -> int32 {
    auto const n{get_num_elements()};
    int32 count{0};

    for (int32 i{0}; i < n; ++i) {
        if (entity_data.alive[i] && (entity_data.teams[i] != team)) { ++count; }
    }

    return count;
}

// Area queries
auto ATestEntityRegistry::collect_entities_in_range(
    FVector3f const& origin,
    float const radius,
    TArrayView<FRegistryEntityHandle> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::collect_entities_in_range);

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

        if (count >= n_out_limit) { break; }
    }

    return count;
}
auto ATestEntityRegistry::collect_non_team_entities_in_range(
    FVector3f const& origin,
    ETestTeam const team,
    float const radius,
    TArrayView<FRegistryEntityHandle> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::collect_non_team_entities_in_range);

    int32 count{0};

    auto const radius_squared{radius * radius};
    auto const n{get_num_elements()};
    auto const n_out_limit{out_entities.Num()};

    auto const ox{origin.X};
    auto const oy{origin.Y};
    auto const oz{origin.Z};

    for (int32 i{0}; i < n; ++i) {
        if (!entity_data.alive[i]) { continue; }
        if (entity_data.teams[i] == team) { continue; }

        auto const dist_sq{ml::dist_sq(entity_data.locations, i, ox, oy, oz)};
        if (dist_sq > radius_squared) { continue; }
        out_entities[count++] = FRegistryEntityHandle{i, generations[i]};

        if (count >= n_out_limit) { break; }
    }

    return count;
}
void ATestEntityRegistry::are_entities_within_dist_sq(float const dist_sq_threshold,
                                                      FVectors3f const& locations,
                                                      TArrayView<bool> results) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::are_entities_within_dist_sq);

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(locations),
        SANDBOX_NAMED_NUM(results),
    });
    ml::fill(results, false);

    auto const n_inputs{ml::num(locations)};
    auto const n_entities{ml::num(entity_data.locations)};

    for (int32 input_i{0}; input_i < n_inputs; ++input_i) {
        float x{locations.xs[input_i]};
        float y{locations.zs[input_i]};
        float z{locations.ys[input_i]};

        for (int32 entity_i{0}; entity_i < n_entities; ++entity_i) {
            if (!entity_data.alive[entity_i]) { continue; }

            float const dist_sq{ml::dist_sq(entity_data.locations, entity_i, x, y, x)};
            if (dist_sq <= dist_sq_threshold) {
                results[input_i] = true;
                break;
            }
        }
    }
}
auto ATestEntityRegistry::get_any_non_team_entity(ETestTeam const team) const
    -> FRegistryEntityHandle {
    auto const n{get_num_elements()};

    for (int32 i{0}; i < n; ++i) {
        if (!entity_data.alive[i]) { continue; }
        if (entity_data.teams[i] != team) { return {i, generations[i]}; }
    }

    return {};
}
auto ATestEntityRegistry::get_any_non_team_entity(ETestTeam const team,
                                                  ETestEntityType const entity_type) const
    -> FRegistryEntityHandle {
    auto const n{get_num_elements()};

    for (int32 i{0}; i < n; ++i) {
        if (!entity_data.alive[i]) { continue; }
        if (entity_data.teams[i] == team) { continue; }
        if (entity_data.entity_types[i] != entity_type) { continue; }

        return {i, generations[i]};
    }

    return {};
}

// Checks
void ATestEntityRegistry::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_data),
        SANDBOX_NAMED_NUM(generations),
        SANDBOX_NAMED_NUM(unique_ids),
    });

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(entity_owners),
        SANDBOX_NAMED_NUM(queued_damage_events),
    });

    entity_data.validate_array_sizes();

    auto const num_ids_issued{static_cast<int32>(get_num_unique_ids_issued())};
    if (unique_entities.num() != num_ids_issued) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("%d unique ids != %d ids issued"),
               unique_entities.num(),
               num_ids_issued);
    }
}
void ATestEntityRegistry::validate_unique_ids() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::validate_unique_ids);

    // Development-time validation for the unique id system
    auto const n{unique_ids.Num()};

    for (int32 i{0}; i < n; ++i) {
        if (!is_valid_unique_id(unique_ids[i])) {
            UE_LOG(LogSandbox,
                   Fatal,
                   TEXT("Invalid unique id detected: (id[%d] = %u)"),
                   i,
                   unique_ids[i].id);
        }
    }
}
void ATestEntityRegistry::validate_unique_entity_data() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::validate_death_reasons);

    auto const n{unique_entities.num()};

    for (int32 i{0}; i < n; ++i) {
        FRegistryEntityHandle const handle{
            unique_entities.registry_indices[i],
            unique_entities.registry_generations[i],
        };
        auto const handle_status{analyse_handle(handle)};
        check(handle_status != ERegistryHandleState::Invalid);
        check(handle_status != ERegistryHandleState::Null);

        if (unique_entities.alive[i]) { continue; }
        check(unique_entities.death_reason[i] != ETestDeathReason::Unset);
    }
}
