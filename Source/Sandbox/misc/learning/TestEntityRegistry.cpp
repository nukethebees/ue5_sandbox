#include "TestEntityRegistry.h"

#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/utilities/actor_utils.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

// -------------------------------------------------------------------------------------------------
// TraceHits
// -------------------------------------------------------------------------------------------------
void TraceHits::reset() {
    ml::reset(actors, actor_components, hit_items);
}
auto TraceHits::num() const -> int32 {
    return actors.Num();
}
void TraceHits::remove_at_swap(int32 const index,
                               int32 const count,
                               EAllowShrinking const allow_shrinking) {
    actors.RemoveAtSwap(index, count, allow_shrinking);
    actor_components.RemoveAtSwap(index, count, allow_shrinking);
    hit_items.RemoveAtSwap(index, count, allow_shrinking);
}
void TraceHits::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(actors),
        SANDBOX_NAMED_NUM(actor_components),
        SANDBOX_NAMED_NUM(hit_items),
    });
}

// -------------------------------------------------------------------------------------------------
// DamageEvents
// -------------------------------------------------------------------------------------------------
auto DamageEvents::num() const -> int32 {
    return damage_amounts.Num();
}
void DamageEvents::reset() {
    ml::reset(damage_amounts, actor_components, hit_items);
}
void DamageEvents::remove_at_swap(int32 const index,
                                  int32 const count,
                                  EAllowShrinking const allow_shrinking) {
    damage_amounts.RemoveAtSwap(index, count, allow_shrinking);
    actor_components.RemoveAtSwap(index, count, allow_shrinking);
    hit_items.RemoveAtSwap(index, count, allow_shrinking);
}
void DamageEvents::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(damage_amounts),
        SANDBOX_NAMED_NUM(actor_components),
        SANDBOX_NAMED_NUM(hit_items),
    });
}

// -------------------------------------------------------------------------------------------------
// TestEntityUniqueEntityData
// -------------------------------------------------------------------------------------------------
auto TestEntityUniqueEntityData::num() const -> int32 {
    return registry_handles.Num();
}
void TestEntityUniqueEntityData::reset() {
    ml::reset(registry_handles, kills, alive, killed_by);
}
void TestEntityUniqueEntityData::add_defaulted(int32 const count) {
    registry_handles.AddDefaulted(count);
    kills.AddDefaulted(count);
    alive.AddDefaulted(count);
    killed_by.AddDefaulted(count);
}

void TestEntityUniqueEntityData::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(registry_handles),
        SANDBOX_NAMED_NUM(kills),
        SANDBOX_NAMED_NUM(alive),
        SANDBOX_NAMED_NUM(killed_by),
    });
}

// -------------------------------------------------------------------------------------------------
// NewEntities
// -------------------------------------------------------------------------------------------------
auto NewEntities::num() const -> int32 {
    return registry_indices.Num();
}
void NewEntities::reset() {
    ml::reset(registry_indices);
}
void NewEntities::add_defaulted(int32 const count) {
    registry_indices.AddDefaulted(count);
}
void NewEntities::add_uninitialised(int32 const count) {
    registry_indices.AddUninitialized(count);
}

// -------------------------------------------------------------------------------------------------
// EntityDeathInfo
// -------------------------------------------------------------------------------------------------
auto EntityDeathInfo::num() const -> int32 {
    return reasons.Num();
}

void EntityDeathInfo::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(reasons),
        SANDBOX_NAMED_NUM(victims),
        SANDBOX_NAMED_NUM(killers),
    });
}

// -------------------------------------------------------------------------------------------------
// ATestEntityRegistry
// -------------------------------------------------------------------------------------------------
ATestEntityRegistry::ATestEntityRegistry() {
    PrimaryActorTick.bCanEverTick = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

void ATestEntityRegistry::reset() {
    entity_data.reset();
    queued_entity_data.reset();

    ml::reset(generations,
              queued_entity_generations,
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
auto ATestEntityRegistry::reserve_entities(int32 const count) -> NewEntities {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::reserve_entities);

    FTestEntityRegistryEntityData empty_data;
    empty_data.add_disabled(count);

    return add_entities(empty_data.get_const_view());
}
auto ATestEntityRegistry::add_entities(FTestEntityRegistryEntityData::ConstView const view)
    -> NewEntities {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::add_entities);

    auto const count{view.get_num()};

    NewEntities new_entities;
    new_entities.first_id = {
        .id = unique_entities.num(),
    };
    unique_entities.add_defaulted(count);
    auto const first_id{new_entities.first_id};

    auto const n_free_indices{free_indices.Num()};
    auto const free_to_reserve{FMath::Min(n_free_indices, count)};

    int32 new_entity_index{0};
    for (int32 i{0}; i < free_to_reserve; ++i) {
        auto const index{free_indices.Pop(EAllowShrinking::No)};

        ml::assign_from(entity_data.locations, index, view.locations, i);
        ml::assign_from(entity_data.velocities, index, view.velocities, i);

        entity_data.healths[index] = view.healths[i];
        entity_data.teams[index] = view.teams[i];
        entity_data.alive[index] = view.alive[i];

        ++generations[index];
        unique_ids[index] = first_id + new_entity_index;

        new_entities.registry_indices.Emplace(index, generations[index]);
        new_entity_index++;
    }

    auto const indices_left_to_reserve{count - new_entities.registry_indices.Num()};
    auto start_index{get_num_elements()};

    generations.AddZeroed(indices_left_to_reserve);
    unique_ids.AddDefaulted(indices_left_to_reserve);

    entity_data.add(view.get_slice(ml::num(new_entities), indices_left_to_reserve));

    for (int32 i{0}; i < indices_left_to_reserve; ++i) {
        auto const index{start_index + i};

        new_entities.registry_indices.Emplace(index, generations[index]);
        unique_ids[index] = first_id + new_entity_index;

        new_entity_index++;
    }

    validate_array_sizes();
    validate_unique_ids();

    return new_entities;
}

// Damage updates
void ATestEntityRegistry::queue_damage_events(TConstArrayView<int32> const damages,
                                              TConstArrayView<AActor*> const actors,
                                              TConstArrayView<UActorComponent*> const components,
                                              TConstArrayView<int32> const items) {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(damages),
        SANDBOX_NAMED_NUM(actors),
        SANDBOX_NAMED_NUM(components),
        SANDBOX_NAMED_NUM(items),
    });

    auto const n{ml::num(damages)};

    for (int32 i{0}; i < n; ++i) {
        auto const id{get_owner(actors[i])};
        if (!id.is_valid()) { continue; }

        auto& actor_damage_events{queued_damage_events[id.id]};
        actor_damage_events.damage_amounts.Add(damages[i]);
        actor_damage_events.actor_components.Add(components[i]);
        actor_damage_events.hit_items.Add(items[i]);
    }
}
auto ATestEntityRegistry::get_damage_queue_view(TestEntityOwnerId const id) const
    -> DamageEvents const& {
    check(is_valid_owner(id));
    return queued_damage_events[id.id];
}

// Entity updates
void ATestEntityRegistry::update_entities(ConstView const view) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::update_entities);

    queued_entity_data.add(view.data);
    queued_entity_generations.Append(view.indices);
}
void ATestEntityRegistry::set_death_info(EntityDeathInfo const& death_info) {
    death_info.validate_array_sizes();

    auto const n{death_info.num()};
    if (num < 1) { return; }
}
void ATestEntityRegistry::commit_entity_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_entity_updates);

    auto const n{queued_entity_data.get_num()};

    for (int32 i{0}; i < n; ++i) {
        auto const generation_index{queued_entity_generations[i]};
        if (!is_valid_index(generation_index)) { continue; }
        auto const entity_index{generation_index.index};

        ml::assign_from(entity_data.locations, entity_index, queued_entity_data.locations, i);
        ml::assign_from(entity_data.velocities, entity_index, queued_entity_data.velocities, i);
        entity_data.healths[entity_index] = queued_entity_data.healths[i];
        entity_data.teams[entity_index] = queued_entity_data.teams[i];
        entity_data.alive[entity_index] = queued_entity_data.alive[i];
    }
}
void ATestEntityRegistry::commit_death_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_death_updates);

    dead_entities_this_frame.Reset();
    auto const n{entity_data.get_num()};
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
    auto const n{entity_data.get_num()};
    for (int32 i{0}; i < n; ++i) {
        if (entity_data.alive[i] == 0u) { free_indices.Add(i); }
    }
}
void ATestEntityRegistry::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::end_tick);

    refresh_free_indices();

    ml::reset(queued_entity_data,
              queued_entity_generations,
              queued_entity_generations,
              dead_entities_this_frame);

    auto const n_owners{entity_owners.Num()};
    for (int32 i{0}; i < n_owners; ++i) {
        ml::reset(queued_damage_events[i]);
    }

    validate_array_sizes();
    validate_unique_ids();
}

// Index queries
auto ATestEntityRegistry::is_valid_index(FRegistryEntityHandle const index) const -> bool {
    return generations.IsValidIndex(index.index) && (generations[index.index] == index.generation);
}
auto ATestEntityRegistry::is_stale(FRegistryEntityHandle const index) const -> bool {
    return generations.IsValidIndex(index.index) && (generations[index.index] > index.generation);
}

// Unique id queries
auto ATestEntityRegistry::is_valid_unique_id(TestEntityUniqueId const id) const -> bool {
    return id.is_valid() && (id.id < get_num_unique_ids_issued());
}

auto ATestEntityRegistry::get_kills(TestEntityUniqueId const id) const
    -> TestEntityUniqueEntityData::kills_type {
    check(is_valid_unique_id(id));
    return unique_entities.kills[id.id];
}

// Entity queries
auto ATestEntityRegistry::get_location(FRegistryEntityHandle const index) const -> FVector3f {
    check(is_valid_index(index));
    return ml::get_vector3f(entity_data.locations, index.index);
}
auto ATestEntityRegistry::get_velocity(FRegistryEntityHandle const index) const -> FVector3f {
    check(is_valid_index(index));
    return ml::get_vector3f(entity_data.velocities, index.index);
}
auto ATestEntityRegistry::get_health(FRegistryEntityHandle const index) const -> int32 {
    check(is_valid_index(index));
    return entity_data.healths[index.index];
}
auto ATestEntityRegistry::get_team(FRegistryEntityHandle const index) const -> ETestTeam {
    check(is_valid_index(index));
    return entity_data.teams[index.index];
}
auto ATestEntityRegistry::get_alive(FRegistryEntityHandle const index) const -> bool {
    check(is_valid_index(index));
    return static_cast<bool>(entity_data.alive[index.index]);
}
auto ATestEntityRegistry::get_dead_entities_this_frame() const
    -> TConstArrayView<FRegistryEntityHandle> {
    return dead_entities_this_frame;
}
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
        auto const dist_sq{ml::dist_sq(entity_data.locations, i, ox, oy, oz)};

        if (dist_sq > radius_squared) { continue; }

        if (entity_data.teams[i] == team) { continue; }

        out_entities[count++] = FRegistryEntityHandle{i, generations[i]};

        if (count >= n_out_limit) { break; }
    }

    return count;
}
auto ATestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return entity_data.get_num();
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
