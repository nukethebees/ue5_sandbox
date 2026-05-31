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
