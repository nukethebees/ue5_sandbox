#include <SandboxCore/transforms.h>

#include <CoreMinimal.h>

namespace ml {
auto make_transforms(::ioj::sim::Vectors3fConstView const locations,
                     ::ioj::sim::Rotators3fConstView const rotations) -> TArray<FTransform> {
    auto const n{locations.num()};
    check(n == rotations.num());

    TArray<FTransform> out;
    out.Reserve(n);
    for (int32 i{}; i < n; ++i) {
        out.Emplace(FRotator3d{rotations.pitches[i], rotations.yaws[i], rotations.rolls[i]},
                    FVector3d{locations.xs[i], locations.ys[i], locations.zs[i]});
    }

    return out;
}

void set_transform_locations(TArrayView<FTransform> const transforms,
                             ::ioj::sim::Vectors3fConstView const locations) {
    auto const n{transforms.Num()};

    check(n == locations.num());

    for (int32 i{0}; i < n; ++i) {
        transforms[i].SetLocation(FVector3d{locations.xs[i], locations.ys[i], locations.zs[i]});
    }
}

}
