#include "TestEntityRegistry.h"

#include "Sandbox/utilities/actor_utils.h"

ATestEntityRegistry::ATestEntityRegistry() {
    PrimaryActorTick.bCanEverTick = false;

    ml::set_actor_component_mobility(*this, EComponentMobility::Static);
}

// Entity creation
auto ATestEntityRegistry::reserve_entities(int32 const count) -> TArray<FGenerationIndex> {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::reserve_entities);

    TArray<FGenerationIndex> indices;

    auto const n_free_indices{free_indices.Num()};
    auto const free_to_reserve{FMath::Min(n_free_indices, count)};

    for (int32 i{0}; i < free_to_reserve; ++i) {
        auto const index{free_indices.Pop(EAllowShrinking::No)};

        entity_data.locations[index] = FVector::ZeroVector;
        entity_data.velocities[index] = FVector::ZeroVector;
        entity_data.healths[index] = 0;
        entity_data.teams[index] = ETestTeam::neutral;
        entity_data.alive[index] = false;

        ++generations[index];

        indices.Emplace(index, generations[index]);
    }

    auto const indices_left_to_reserve{count - indices.Num()};
    auto start_index{get_num_elements()};

    entity_data.add_disabled(indices_left_to_reserve);
    generations.AddZeroed(indices_left_to_reserve);

    for (int32 i{0}; i < indices_left_to_reserve; ++i) {
        auto const index{start_index + i};
        indices.Emplace(index, generations[index]);
    }

    return indices;
}
auto ATestEntityRegistry::add_entities(FTestEntityRegistryEntityData::ConstView const view)
    -> TArray<FGenerationIndex> {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::add_entities);

    TArray<FGenerationIndex> indices;

    auto const count{view.get_num()};
    auto const n_free_indices{free_indices.Num()};
    auto const free_to_reserve{FMath::Min(n_free_indices, count)};

    for (int32 i{0}; i < free_to_reserve; ++i) {
        auto const index{free_indices.Pop(EAllowShrinking::No)};

        entity_data.locations[index] = view.locations[i];
        entity_data.velocities[index] = view.velocities[i];
        entity_data.healths[index] = view.healths[i];
        entity_data.teams[index] = view.teams[i];
        entity_data.alive[index] = view.alive[i];

        ++generations[index];

        indices.Emplace(index, generations[index]);
    }

    auto const indices_left_to_reserve{count - indices.Num()};
    auto start_index{get_num_elements()};

    generations.AddZeroed(indices_left_to_reserve);
    entity_data.add(view.get_slice(indices.Num(), indices_left_to_reserve));

    for (int32 i{0}; i < indices_left_to_reserve; ++i) {
        auto const index{start_index + i};
        indices.Emplace(index, generations[index]);
    }

    return indices;
}

// Entity updates
void ATestEntityRegistry::update_entities(ConstView const view) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::update_entities);

    queued_entity_data.add(view.data);
    queued_entity_generations.Append(view.indices);
}
void ATestEntityRegistry::apply_damage(TConstArrayView<FGenerationIndex> const indexes,
                                       TConstArrayView<int32> const damages) {
    check(indexes.Num() == damages.Num());

    queued_damage_targets.Append(indexes);
    queued_damage_amounts.Append(damages);
}
void ATestEntityRegistry::commit_entity_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_entity_updates);

    auto const n{queued_entity_data.get_num()};

    for (int32 i{0}; i < n; ++i) {
        auto const generation_index{queued_entity_generations[i]};
        if (!is_valid_index(generation_index)) {
            continue;
        }
        auto const entity_index{generation_index.index};

        entity_data.locations[entity_index] = queued_entity_data.locations[i];
        entity_data.velocities[entity_index] = queued_entity_data.velocities[i];
        entity_data.healths[entity_index] = queued_entity_data.healths[i];
        entity_data.teams[entity_index] = queued_entity_data.teams[i];
        entity_data.alive[entity_index] = queued_entity_data.alive[i];
    }

    queued_entity_data.reset();
    queued_entity_generations.Reset();
}
void ATestEntityRegistry::commit_damage_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_damage_updates);

    auto const n{queued_damage_amounts.Num()};

    for (int32 i{0}; i < n; ++i) {
        auto const generation_index{queued_damage_targets[i]};
        if (!is_valid_index(generation_index)) {
            continue;
        }
        auto const entity_index{generation_index.index};

        entity_data.healths[entity_index] -= queued_damage_amounts[i];
        entity_data.alive[entity_index] = (entity_data.healths[entity_index] > 0);
    }

    queued_damage_amounts.Reset();
    queued_damage_targets.Reset();
}
void ATestEntityRegistry::commit_death_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_death_updates);

    dead_entities_this_frame.Reset();
    auto const n{entity_data.get_num()};
    for (int32 i{0}; i < n; ++i) {
        if (entity_data.alive[i] == 0u) {
            dead_entities_this_frame.Emplace(i, generations[i]);
        }
    }
}

// Frame events
void ATestEntityRegistry::commit_updates() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::commit_updates);

    commit_entity_updates();
    commit_damage_updates();
    commit_death_updates();
}
void ATestEntityRegistry::refresh_free_indices() {
    free_indices.Reset();
    auto const n{entity_data.get_num()};
    for (int32 i{0}; i < n; ++i) {
        if (entity_data.alive[i] == 0u) {
            free_indices.Add(i);
        }
    }
}
void ATestEntityRegistry::end_frame() {
    refresh_free_indices();
    dead_entities_this_frame.Reset();
}

// Entity queries
auto ATestEntityRegistry::is_valid_index(FGenerationIndex const index) const -> bool {
    return generations.IsValidIndex(index.index) && (generations[index.index] == index.generation);
}
auto ATestEntityRegistry::get_location(FGenerationIndex const index) const -> FVector {
    return is_valid_index(index) ? entity_data.locations[index.index] : FVector::ZeroVector;
}
auto ATestEntityRegistry::get_velocity(FGenerationIndex const index) const -> FVector {
    return is_valid_index(index) ? entity_data.velocities[index.index] : FVector::ZeroVector;
}
auto ATestEntityRegistry::get_health(FGenerationIndex const index) const -> int32 {
    return is_valid_index(index) ? entity_data.healths[index.index] : -1;
}
auto ATestEntityRegistry::get_team(FGenerationIndex const index) const -> ETestTeam {
    return is_valid_index(index) ? entity_data.teams[index.index] : ETestTeam::neutral;
}
auto ATestEntityRegistry::get_alive(FGenerationIndex const index) const -> bool {
    return is_valid_index(index) ? static_cast<bool>(entity_data.alive[index.index]) : false;
}
auto ATestEntityRegistry::get_dead_entities_this_frame() const
    -> TConstArrayView<FGenerationIndex> {
    return dead_entities_this_frame;
}
auto ATestEntityRegistry::collect_entities_in_range(
    FVector const& origin,
    float const radius,
    TArrayView<FGenerationIndex> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::collect_entities_in_range);

    int32 count{0};

    auto const radius_squared{radius * radius};
    auto const n{get_num_elements()};
    auto const n_out_limit{out_entities.Num()};

    for (int32 i{0}; i < n; ++i) {
        auto const dist_sq{FVector::DistSquared(origin, entity_data.locations[i])};
        if (dist_sq <= radius_squared) {
            out_entities[count++] = FGenerationIndex{i, generations[i]};
        }

        if (count >= n_out_limit) {
            break;
        }
    }

    return count;
}
auto ATestEntityRegistry::collect_non_team_entities_in_range(
    FVector const& origin,
    ETestTeam const team,
    float const radius,
    TArrayView<FGenerationIndex> const out_entities) const -> int32 {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::collect_non_team_entities_in_range);

    int32 count{0};

    auto const radius_squared{radius * radius};
    auto const n{get_num_elements()};
    auto const n_out_limit{out_entities.Num()};

    for (int32 i{0}; i < n; ++i) {
        auto const dist_sq{FVector::DistSquared(origin, entity_data.locations[i])};
        if (dist_sq > radius_squared) {
            continue;
        }

        if (entity_data.teams[i] == team) {
            continue;
        }

        out_entities[count++] = FGenerationIndex{i, generations[i]};

        if (count >= n_out_limit) {
            break;
        }
    }

    return count;
}
auto ATestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return entity_data.get_num();
}
