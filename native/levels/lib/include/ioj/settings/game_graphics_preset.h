#pragma once

#include <cstdint>

namespace ioj {
enum class GameGraphicsPreset : std::uint8_t {
    Custom,
    Low,
    Medium,
    High,
    Epic,
};
}
