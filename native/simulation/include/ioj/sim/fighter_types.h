#pragma once

#include <ioj/sim/fighter_orders.h>
#include <string_view>

namespace ioj::sim {
[[nodiscard]] constexpr auto to_string_view(FighterTask const task) noexcept -> std::string_view {
    switch (task) {
        case FighterTask::Standby: {
            return "Standby";
        }
        case FighterTask::MoveToDestination: {
            return "MoveToDestination";
        }
        case FighterTask::Attack: {
            return "Attack";
        }
        case FighterTask::COUNT: {
            return "COUNT";
        }
    }

    return "Unknown";
}
}
