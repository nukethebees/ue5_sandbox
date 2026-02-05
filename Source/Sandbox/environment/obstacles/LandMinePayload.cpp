#include "Sandbox/environment/obstacles/LandMinePayload.h"

#include "Engine/World.h"

#include "Sandbox/combat/explosion/ExplosionSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void FLandMinePayload::execute(FCollisionContext context) {
    explode(context.world);
}
void FLandMinePayload::explode(UWorld& world) {
    TRY_INIT_PTR(explosion_subsystem, world.GetSubsystem<UExplosionSubsystem>());

    auto trigger_explosion{[explosion_subsystem, *this]() {
        explosion_subsystem->spawn_explosion(
            mine_location, FRotator::ZeroRotator, explosion_config);
    }};

    // Start timer to trigger explosion after delay to synchronize with destruction
    if (detonation_delay > 0.0f) {
        world.GetTimerManager().SetTimer(timer_handle, trigger_explosion, detonation_delay, false);
    } else {
        // No delay, spawn immediately
        trigger_explosion();
    }
}
