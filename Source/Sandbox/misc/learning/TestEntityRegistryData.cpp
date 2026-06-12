#include "TestEntityRegistryData.h"

#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_vector_utils.h>

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
    ml::fill(slice.velocities, 0.f);
    ml::fill(slice.healths, 0);
    ml::fill(slice.teams, ETestTeam::neutral);
    ml::fill(slice.alive, uint8{0u});
}
void FTestEntityRegistryEntityData::add(ConstView const view) {
    locations.Append(view.locations);

    ml::append_from(velocities, view.velocities);

    healths.Append(view.healths);
    teams.Append(view.teams);
    alive.Append(view.alive);
}

void FTestEntityRegistryEntityData::reset() {
    ml::reset(locations, velocities, healths, teams, alive);
}
