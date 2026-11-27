#pragma once

#include <ioj/sim/player/player_input_frame.h>

#include <vector>

namespace ioj {
struct LogicalInputState;

auto ship_input_frame(LogicalInputState const& state,
                      std::vector<sim::player::PlayerInputKind>& events)
    -> sim::player::PlayerInputFrame;
}
