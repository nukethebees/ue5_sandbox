#pragma once
#include <ioj/sim/player/player_event_kind.h>
#include <ioj/sim/player/player_input_kind.h>

#include <memory_resource>
#include <vector>

namespace ioj::sim::player {
struct PlayerInputEvents {
    void add(PlayerInputKind kind) {
        if (inputs.size() < 255) {
            inputs.push_back(kind);
        }
    }

    std::pmr::vector<PlayerInputKind> inputs;
};
using PlayerEvents = std::pmr::vector<PlayerEventKind>;
}
