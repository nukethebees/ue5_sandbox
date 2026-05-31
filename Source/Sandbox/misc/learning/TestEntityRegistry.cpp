#include "TestEntityRegistry.h"

ATestEntityRegistry::ATestEntityRegistry() {
    PrimaryActorTick.bCanEverTick = false;
}

auto ATestEntityRegistry::collect_entities_in_range(FVector const& origin,
                                                    float radius,
                                                    TArrayView<FGenerationIndex> out_entities)
    -> int32 {
    return 0;
}
