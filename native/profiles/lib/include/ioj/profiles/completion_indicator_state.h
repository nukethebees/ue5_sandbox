#pragma once

#include <cstdint>

namespace ioj::profiles {
enum class CompletionIndicatorState : std::uint8_t { Incomplete, Completed, ParAchieved };
}
