#pragma once
#include <ioj/sim/entity_unique_id.h>
#include <ioj/sim/rotator_types.h>
#include <ioj/sim/team.h>
#include <ioj/sim/vector_types.h>

namespace ioj::sim {
enum class EntityFrameChangeKind : std::uint8_t { Spawn, RemoveSwap };

struct EntityFrameChange {
    EntityFrameChangeKind kind{};
    std::uint32_t index{};
    Vector3f location{};
    Rotator3f rotation{};
    Team team{};
    EntityUniqueId id{};
};

} // namespace ioj::sim
