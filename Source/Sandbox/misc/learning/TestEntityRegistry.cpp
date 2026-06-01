#include "TestEntityRegistry.h"

ATestEntityRegistry::ATestEntityRegistry() {
    PrimaryActorTick.bCanEverTick = false;
}

auto ATestEntityRegistry::collect_entities_in_range(FVector const& origin,
                                                    float radius,
                                                    TArrayView<FGenerationIndex> out_entities)
    -> int32 {
    int32 count{0};

    auto const radius_squared{radius * radius};
    auto const n{get_num_elements()};
    auto const n_out_limit{out_entities.Num()};

    for (int32 i{0}; i < n; ++i) {
        if (FVector::DistSquared(origin, locations[i]) <= radius_squared) {
            out_entities[count++] = FGenerationIndex{i, generations[i]};

            if (count >= n_out_limit) {
                break;
            }
        }
    }

    return count;
}
auto ATestEntityRegistry::get_num_elements() const noexcept -> int32 {
    return locations.Num();
}

auto ATestEntityRegistry::reserve_entities(int32 const count) -> TArray<FGenerationIndex> {
    TArray<FGenerationIndex> indices;

    auto const n_free_indices{free_indices.Num()};
    auto const free_to_reserve{FMath::Min(n_free_indices, count)};

    for (int32 i{}; i < free_to_reserve; ++i) {
        auto const index{free_indices.Pop(EAllowShrinking::No)};

        locations[index] = FVector::ZeroVector;
        velocities[index] = FVector::ZeroVector;
        healths[index] = 0;
        ++generations[index];
        teams[index] = ETestTeam::neutral;
        alive[index] = false;

        indices.Emplace(index, generations[index]);
    }

    auto const indices_left_to_reserve{count - indices.Num()};
    auto start_index{get_num_elements()};

    locations.AddUninitialized(indices_left_to_reserve);
    velocities.AddUninitialized(indices_left_to_reserve);
    healths.AddUninitialized(indices_left_to_reserve);
    generations.AddUninitialized(indices_left_to_reserve);
    teams.AddUninitialized(indices_left_to_reserve);
    alive.AddUninitialized(indices_left_to_reserve);

    for (int32 i{0}; i < indices_left_to_reserve; ++i) {
        auto const index{start_index + i};

        locations[index] = FVector::ZeroVector;
        velocities[index] = FVector::ZeroVector;
        healths[index] = 0;
        generations[index] = 0;
        teams[index] = ETestTeam::neutral;
        alive[index] = false;
        indices.Emplace(index, generations[index]);
    }

    return indices;
}
