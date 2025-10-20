#include "Sandbox/environment/obstacles/data/LandMinePayload.h"

#include "Engine/World.h"

#include "Sandbox/combat/effects/actors/Explosion.h"
#include "Sandbox/health/data/HealthChange.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void FLandMinePayload::execute(FCollisionContext context) {
    static constexpr auto logger{log.NestedLogger<"execute">()};

    auto spawn_explosion{[&world = context.world, *this]() {
        FActorSpawnParameters spawn_params;
        spawn_params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        auto* explosion{world.SpawnActor<AExplosion>(
            explosion_class, mine_location, FRotator::ZeroRotator, spawn_params)};

        if (explosion) {
            explosion->damage_config = FHealthChange{damage, EHealthChangeType::Damage};
            explosion->explosion_radius = explosion_radius;
            explosion->explosion_force = explosion_force;
        } else {
            logger.log_warning(TEXT("Failed to spawn explosion actor."));
        }
    }};

    // Start timer to trigger explosion after delay to synchronize with destruction
    if (detonation_delay > 0.0f) {
        context.world.GetTimerManager().SetTimer(
            timer_handle, spawn_explosion, detonation_delay, false);
    } else {
        // No delay, spawn immediately
        spawn_explosion();
    }
}
