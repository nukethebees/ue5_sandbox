#pragma once
#include <cstdint>

namespace ioj::sim {
enum class OrchestratorState : std::uint8_t {
    Uninitialised,
    Paused,
    Running,
    Stopped,
};
}
