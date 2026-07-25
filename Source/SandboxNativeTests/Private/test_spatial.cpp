#include "SandboxNative/spatial.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxNative.GetRandomPoint") {
    FVector centre{};
    constexpr float min_dist{100.f};
    constexpr float max_dist{1000.f};
    constexpr int32 cases{1000};

    for (int32 i{0}; i < cases; ++i) {
        auto const pos{ml::get_random_point(centre, min_dist, max_dist)};
        auto const dist{FVector::Dist(centre, pos)};
        REQUIRE(dist >= min_dist);
        REQUIRE(dist <= max_dist);
    }
}
