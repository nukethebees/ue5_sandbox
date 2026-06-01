#include "TestEntityRegistry.h"

auto FTestEntityRegistryEntityData::get_num() const -> int32 {
    return locations.Num();
}
auto FTestEntityRegistryEntityData::get_view() -> View {
    return {
        locations,
        velocities,
        healths,
        teams,
        alive,
    };
}
auto FTestEntityRegistryEntityData::get_const_view() const -> ConstView {
    return {
        locations,
        velocities,
        healths,
        teams,
        alive,
    };
}

void FTestEntityRegistryEntityData::add_uninitialised(int32 const count) {
    locations.AddUninitialized(count);
    velocities.AddUninitialized(count);
    healths.AddUninitialized(count);
    teams.AddUninitialized(count);
    alive.AddUninitialized(count);
}
void FTestEntityRegistryEntityData::add_disabled(int32 const count) {
    auto const base{get_num()};

    add_uninitialised(count);

    for (int32 i{0}; i < count; ++i) {
        auto const index{base + i};

        locations[index] = FVector::ZeroVector;
        velocities[index] = FVector::ZeroVector;
        healths[index] = 0;
        teams[index] = ETestTeam::neutral;
        alive[index] = false;
    }
}
void FTestEntityRegistryEntityData::add(ConstView const view) {
    auto const n{view.get_num()};
    auto const base{get_num()};

    add_uninitialised(n);
    for (int32 i{0}; i < n; ++i) {
        auto const index{base + i};

        locations[index] = view.locations[i];
        velocities[index] = view.velocities[i];
        healths[index] = view.healths[i];
        teams[index] = view.teams[i];
        alive[index] = view.alive[i];
    }
}

ATestEntityRegistry::ATestEntityRegistry() {
    PrimaryActorTick.bCanEverTick = false;
}

// Entity updates
auto ATestEntityRegistry::reserve_entities(int32 const count) -> TArray<FGenerationIndex> {
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
auto ATestEntityRegistry::collect_entities_in_range(FVector const& origin,
                                                    float radius,
                                                    TArrayView<FGenerationIndex> out_entities) const
    -> int32 {
    int32 count{0};

    auto const radius_squared{radius * radius};
    auto const n{get_num_elements()};
    auto const n_out_limit{out_entities.Num()};

    for (int32 i{0}; i < n; ++i) {
        if (FVector::DistSquared(origin, entity_data.locations[i]) <= radius_squared) {
            out_entities[count++] = FGenerationIndex{i, generations[i]};

            if (count >= n_out_limit) {
                break;
            }
        }
    }

    return count;
}
auto ATestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return entity_data.get_num();
}
