#pragma once

#include <cstdint>

namespace ioj::sim {
enum class MissionState : std::uint8_t {
    NotStarted,
    Running,
    Succeeded,
    Failed,
    Disabled,
};
}
