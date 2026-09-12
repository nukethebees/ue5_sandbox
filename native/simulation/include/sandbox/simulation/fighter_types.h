#pragma once

#include <cstdint>
#include <string_view>

namespace ml::simulation {
struct CapitalShipFighterOrder {
    std::uint8_t task   : 1 {0};
    std::uint8_t target : 1 {0};
};

enum class CapitalShipFighterTask : std::uint8_t {
    Standby,
    MoveToDestination,
    Attack,
    COUNT,
};

[[nodiscard]] constexpr auto to_string_view(CapitalShipFighterTask const task) noexcept
    -> std::string_view {
    switch (task) {
        case CapitalShipFighterTask::Standby: {
            return "Standby";
        }
        case CapitalShipFighterTask::MoveToDestination: {
            return "MoveToDestination";
        }
        case CapitalShipFighterTask::Attack: {
            return "Attack";
        }
        case CapitalShipFighterTask::COUNT: {
            return "COUNT";
        }
    }

    return "Unknown";
}
}
