#include "TestEntityRegistryData.h"

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
