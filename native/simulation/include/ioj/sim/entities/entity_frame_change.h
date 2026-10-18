#pragma once
#include <ioj/levels/team.h>
#include <ioj/math_types.h>
#include <ioj/rotator_types.h>
#include <ioj/sim/entities/entity_id.h>

namespace ioj::sim {
enum class EntityFrameChangeKind : std::uint8_t { Spawn, RemoveSwap };

struct EntityFrameChange {
    EntityFrameChangeKind kind{};
    std::uint32_t index{};
    Vector3f location{};
    Rotator3f rotation{};
    Team team{};
    EntityId id{};
};

} // namespace ioj::sim
