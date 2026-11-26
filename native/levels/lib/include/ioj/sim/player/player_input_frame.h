#pragma once

#include <ioj/sim/player/player_input_kind.h>

#include <span>

namespace ioj::sim::player {
struct PlayerInputFrame {
    std::span<PlayerInputKind const> events;
    float pitch;
    float yaw;
    float accelerator;
};
}
