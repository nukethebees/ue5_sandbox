#pragma once

#include <ioj/sim/entities/entity_unique_id.h>
#include <ioj/sim/entities/health_type.h>
#include <ioj/levels/team.h>

#include <array>
#include <cstdint>

namespace ioj::sim {
struct ThinkingEntityState {
    EntityUniqueId id;
    std::uint32_t row{};
    Health health{};
    Team team{};
    std::array<double, 3> location{};
    std::array<double, 4> orientation{};

    auto operator==(ThinkingEntityState const&) const -> bool = default;
};
}
