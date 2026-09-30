#pragma once

#include <ioj/sim/entity_unique_id.h>

#include <algorithm>
#include <span>

namespace ioj::sim {
[[nodiscard]] inline auto check_valid_ids(std::span<EntityUniqueId const> const ids,
                                          EntityType const type) noexcept -> bool {
    return std::ranges::all_of(
        ids, [type](EntityUniqueId const id) { return id.is_valid() && id.entity_type() == type; });
}
}
