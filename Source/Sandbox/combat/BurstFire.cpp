#include "BurstFire.h"

void FBurstFire::tick(float dt) {
    switch (state) {
        case EBurstFireState::ready: {
            break;
        }
        case EBurstFireState::shot_cooldown: {
            shot_cooldown.tick(dt);
            if (shot_cooldown.is_finished()) {
                state = EBurstFireState::ready;
            }

            break;
        }
        case EBurstFireState::burst_cooldown: {
            burst_cooldown.tick(dt);
            if (burst_cooldown.is_finished()) {
                state = EBurstFireState::ready;
                shots_remaining = burst_size;
            }

            break;
        }
    }
}
auto FBurstFire::can_fire() const -> bool {
    return (state == EBurstFireState::ready) && (shots_remaining > 0);
}
auto FBurstFire::fire() -> bool {
    if (!can_fire()) {
        return false;
    }

    shots_remaining--;

    if (shots_remaining <= 0) {
        state = EBurstFireState::burst_cooldown;
        burst_cooldown.reset();
    } else {
        state = EBurstFireState::shot_cooldown;
        shot_cooldown.reset();
    }

    return true;
}
void FBurstFire::reset() {
    shot_cooldown.reset();
    burst_cooldown.reset();
    shots_remaining = burst_size;
    state = EBurstFireState::ready;
}
