#pragma once

#include <cstdint>

namespace ml::simulation {
enum class MissionState : std::uint8_t {
    NotStarted,
    Running,
    Succeeded,
    Failed,
    Disabled,
};
}
