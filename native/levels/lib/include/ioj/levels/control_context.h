#pragma once

#include <cstdint>

namespace ioj::levels {
enum class ControlContext : std::uint8_t { None, Player, Observer, Benchmark };
}
