#pragma once

#include <cstdint>

namespace ioj {
enum class GameWindowMode : std::uint8_t {
    Windowed,
    Borderless,
    Fullscreen,
};
}
