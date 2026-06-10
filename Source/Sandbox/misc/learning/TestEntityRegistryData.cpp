#include "TestEntityRegistryData.h"

#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>

auto FTestEntityRegistryEntityData::get_num() const -> int32 {
    return locations.Num();
}
auto FTestEntityRegistryEntityData::get_view() -> View {
    return {
        locations,
        velocities.get_view(),
        healths,
        teams,
        alive,
    };
}
auto FTestEntityRegistryEntityData::get_const_view() const -> ConstView {
    return {
        locations,
        velocities.get_view(),
        healths,
        teams,
        alive,
    };
}

void FTestEntityRegistryEntityData::add_uninitialised(int32 const count) {
    ml::invoke_on_all([count](auto& a) { ml::add_uninitialised(a, count); },
                      locations,
                      velocities,
                      healths,
                      teams,
                      alive);
}
void FTestEntityRegistryEntityData::add_disabled(int32 const count) {
    add_uninitialised(count);

    auto view{get_view()};
    auto slice{view.right(count)};

    ml::fill(slice.locations, FVector::ZeroVector);
    ml::fill(slice.velocities.xs, 0.f);
    ml::fill(slice.velocities.ys, 0.f);
    ml::fill(slice.velocities.zs, 0.f);
    ml::fill(slice.healths, 0);
    ml::fill(slice.teams, ETestTeam::neutral);
    ml::fill(slice.alive, uint8{0u});
}
void FTestEntityRegistryEntityData::add(ConstView const view) {
    locations.Append(view.locations);

    velocities.xs.Append(view.velocities.xs);
    velocities.ys.Append(view.velocities.ys);
    velocities.zs.Append(view.velocities.zs);

    healths.Append(view.healths);
    teams.Append(view.teams);
    alive.Append(view.alive);
}

void FTestEntityRegistryEntityData::reset() {
    ml::reset(locations, velocities, healths, teams, alive);
}
