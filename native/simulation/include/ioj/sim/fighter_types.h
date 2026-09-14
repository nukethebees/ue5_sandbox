#pragma once

#include <cstdint>
#include <string_view>

namespace ioj::sim {
struct FighterOrder {
    std::uint8_t task   : 1 {0};
    std::uint8_t target : 1 {0};
};

enum class FighterTask : std::uint8_t {
    Standby,
    MoveToDestination,
    Attack,
    COUNT,
};

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
