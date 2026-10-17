#pragma once

// Check authored fighter spawn slots for capital hull intersections and mutual overlap.

#include <ioj/sim/collision/entity_aabbs.h>
#include <ioj/sim/sim_config.h>

#include <cstdint>
#include <vector>

namespace ioj::levels {
using FighterSpawnSlotIndex = std::int32_t;

enum class FighterSpawnSlotValidationErrorKind : std::uint8_t {
    IntersectsCapital,
    SlotsOverlap,
};

struct FighterSpawnSlotValidationError {
    FighterSpawnSlotValidationErrorKind kind{};
    FighterSpawnSlotIndex first_slot{};
    FighterSpawnSlotIndex second_slot{};
    float clearance{};
};

[[nodiscard]] auto validate_fighter_spawn_slots(sim::CapitalShipSimConfig const& capital_config,
                                                sim::FighterSimConfig const& fighter_config,
                                                sim::collision::EntityAABBs const& entity_bounds)
    -> std::vector<FighterSpawnSlotValidationError>;
} // namespace ioj::levels
