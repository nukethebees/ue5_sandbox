#pragma once

#include <ioj/sim/entity_aabbs.h>
#include <ioj/sim/sim_config.h>

#include <cstdint>
#include <vector>

namespace ioj::levels {
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

[[nodiscard]] auto validate_fighter_spawn_slots(sim::CapitalShipSimConfig const& capital_config,
                                                sim::FighterSimConfig const& fighter_config,
                                                sim::collision::EntityAABBs const& entity_bounds)
    -> std::vector<FighterSpawnSlotValidationError>;
} // namespace ioj::levels
