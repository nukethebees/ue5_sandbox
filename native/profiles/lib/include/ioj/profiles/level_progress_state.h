#pragma once

#include <cstdint>

namespace ioj::profiles {
enum class LevelProgressState : std::uint8_t { NotAttempted, Attempted, Completed };
}
