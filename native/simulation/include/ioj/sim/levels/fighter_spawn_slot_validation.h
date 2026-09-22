#pragma once

#include <ioj/sim/entity_aabbs.h>
#include <ioj/sim/sim_config.h>

#include <cstdint>
#include <vector>

namespace ioj::sim::levels {
enum class FighterSpawnSlotValidationErrorKind : std::uint8_t {
    IntersectsCapital,
    SlotsOverlap,
};

struct FighterSpawnSlotValidationError {
    FighterSpawnSlotValidationErrorKind kind{};
    std::int32_t first_slot{};
    std::int32_t second_slot{};
    float clearance{};
};

[[nodiscard]] auto validate_fighter_spawn_slots(CapitalShipSimConfig const& capital_config,
                                                FighterSimConfig const& fighter_config,
                                                collision::EntityAABBs const& entity_bounds)
    -> std::vector<FighterSpawnSlotValidationError>;
} // namespace ioj::sim::levels
