#pragma once

#include <cstdint>

namespace ml::simulation {
enum class OrchestratorState : std::uint8_t {
    Uninitialised,
    Paused,
    Running,
    Stopped,
};
}
