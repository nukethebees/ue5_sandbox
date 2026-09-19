#pragma once

#include "ioj/sim/entity_life_state.h"
#include "ioj/sim/entity_type.h"
#include "ioj/sim/entity_unique_id.h"
#include "ioj/sim/team.h"

#include <cstdint>
#include <limits>

namespace ioj::sim {
struct EntityOwnerId {
    using ThisClass = EntityOwnerId;
    using value_type = std::uint8_t;

    static constexpr value_type NULL_ID{std::numeric_limits<value_type>::max()};

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool { return id != NULL_ID; }

    auto operator==(EntityOwnerId const&) const noexcept -> bool = default;

    value_type id{NULL_ID};
};

}
