#pragma once

#include <ioj/sim/entities/entity_id.h>

#include <algorithm>
#include <span>

namespace ioj::sim {
[[nodiscard]] inline auto check_valid_ids(std::span<EntityId const> const ids,
                                          EntityType const type) noexcept -> bool {
    return std::ranges::all_of(
        ids, [type](EntityId const id) { return id.is_valid() && id.entity_type() == type; });
}
}
