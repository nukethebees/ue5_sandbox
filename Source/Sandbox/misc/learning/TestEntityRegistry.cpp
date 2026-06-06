#include "TestEntityRegistry.h"

ATestEntityRegistry::ATestEntityRegistry() {
    PrimaryActorTick.bCanEverTick = false;
}

// Entity updates
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
void ATestEntityRegistry::update_entities(ConstView const view) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestEntityRegistry::update_entities);

    auto const n{view.get_num()};

    for (int32 i{0}; i < n; ++i) {
        auto const generation_index{view.indices[i]};

        if (!is_valid_index(generation_index)) {
            continue;
        }

        auto const entity_index{generation_index.index};

        entity_data.locations[entity_index] = view.data.locations[i];
        entity_data.velocities[entity_index] = view.data.velocities[i];
        entity_data.healths[entity_index] = view.data.healths[i];
        entity_data.teams[entity_index] = view.data.teams[i];
        entity_data.alive[entity_index] = view.data.alive[i];
    }
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
