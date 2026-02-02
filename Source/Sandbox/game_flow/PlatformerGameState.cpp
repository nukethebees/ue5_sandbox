#include "Sandbox/game_flow/PlatformerGameState.h"

void APlatformerGameState::notify_coin_change(int32 new_coin_count) {
    on_coin_count_changed.Broadcast(new_coin_count);
}
