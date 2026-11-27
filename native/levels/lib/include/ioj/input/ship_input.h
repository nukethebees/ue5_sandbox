#pragma once

#include <ioj/sim/player/player_input_frame.h>

#include <vector>

namespace ioj {
struct InputState;

auto ship_input_frame(InputState const& state, std::vector<sim::player::PlayerInputKind>& events)
    -> sim::player::PlayerInputFrame;
}
