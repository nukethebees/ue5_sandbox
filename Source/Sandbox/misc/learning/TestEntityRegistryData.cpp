#include "TestEntityRegistryData.h"

#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_vector_utils.h>

auto FTestEntityRegistryEntityData::get_num() const -> int32 {
    return ml::num(locations);
}
auto FTestEntityRegistryEntityData::get_view() -> View {
    return {
        locations.get_view(),
        velocities.get_view(),
        healths,
        teams,
        alive,
    };
}
auto FTestEntityRegistryEntityData::get_const_view() const -> ConstView {
    return {
        locations.get_view(),
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

    ml::fill(slice.locations, 0.f);
    ml::fill(slice.velocities, 0.f);
    ml::fill(slice.healths, 0);
    ml::fill(slice.teams, ETestTeam::neutral);
    ml::fill(slice.alive, uint8{0u});
}
void FTestEntityRegistryEntityData::add(ConstView const view) {
    ml::append_from(locations, view.locations);
    ml::append_from(velocities, view.velocities);

    healths.Append(view.healths);
    teams.Append(view.teams);
    alive.Append(view.alive);
}

void FTestEntityRegistryEntityData::reset() {
    ml::reset(locations, velocities, healths, teams, alive);
}

void FTestEntityRegistryEntityData::set_all_alive() {
    ml::fill(alive, uint8{1});
}
void FTestEntityRegistryEntityData::set_all_velocities(float const v) {
    ml::fill(velocities, v);
}

