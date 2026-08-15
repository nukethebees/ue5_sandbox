#include "TestEntityRegistryData.h"

#include <SandboxCore/array_utils.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/soa_vector_utils.h>

namespace ml::entity_registry {
void EntityData::add_disabled(int32 const count) {
    add_uninitialised(count);

    auto view{get_view()};
    auto slice{view.right(count)};

    ml::fill(slice.locations, 0.f);
    ml::fill(slice.velocities, 0.f);
    ml::fill(slice.radii, 0.f);
    ml::fill(slice.healths, 0);
    ml::fill(slice.teams, ETestTeam::White);
    ml::fill(slice.entity_types, ETestEntityType::COUNT);
    ml::fill(slice.alive, uint8{0u});
}
void EntityData::set_all_alive() {
    ml::fill(alive, uint8{1});
}
void EntityData::set_all_velocities(float const v) {
    ml::fill(velocities, v);
}
void EntityData::set_all_entity_types(ETestEntityType const v) {
    ml::fill(entity_types, v);
}
}
