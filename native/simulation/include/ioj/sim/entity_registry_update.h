#pragma once

#include "ioj/sim/entity_history.h"
#include "ioj/sim/entity_registry_bookkeeping.h"
#include "ioj/sim/entity_registry_statistics.h"
#include "ioj/sim/rotators3f.h"
#include "ioj/sim/vectors3f.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
struct EntityRegistryUpdateView {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return locations.num(); }

    Vectors3fView locations;
    Vectors3fView velocities;
    Rotators3fView rotations;
    std::span<std::int32_t> healths;
    std::span<std::byte> teams;
    std::span<std::byte const> entity_types;
    std::span<std::uint8_t> alive;
};

struct EntityRegistryUpdateConstView {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return locations.num(); }

    Vectors3fConstView locations;
    Vectors3fConstView velocities;
    Rotators3fConstView rotations;
    std::span<std::int32_t const> healths;
    std::span<std::byte const> teams;
    std::span<std::uint8_t const> alive;
};

// Applies updates in order and returns the first invalid handle offset, or -1.
[[nodiscard]] auto apply_entity_updates(EntityRegistryBookkeeping& bookkeeping,
                                        EntityRegistryStatistics& statistics,
                                        EntityHistoryColumnsView history,
                                        EntityRegistryUpdateView entities,
                                        EntityRegistryUpdateConstView updates) noexcept
    -> std::int32_t;
} // namespace ioj::sim
