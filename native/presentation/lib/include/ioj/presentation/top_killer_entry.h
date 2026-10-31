#pragma once

#include <ioj/levels/team.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_type.h>

#include <cstdint>

namespace ioj::presentation {
struct TopKillerEntry {
    auto operator==(TopKillerEntry const&) const -> bool = default;

    sim::EntityId entity_id{};
    sim::EntityType entity_type{};
    Team team{};
    std::int32_t kills{};
};
}
