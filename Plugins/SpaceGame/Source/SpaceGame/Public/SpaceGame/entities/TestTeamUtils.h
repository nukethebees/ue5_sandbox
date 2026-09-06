#pragma once

#include <SpaceGame/entities/TestTeam.h>

namespace ml {
constexpr bool is_valid(ETestTeam const team) {
    using T = uint8;
    return static_cast<T>(team) < static_cast<T>(ETestTeam::COUNT);
}
}
