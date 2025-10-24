#include "Sandbox/players/npcs/characters/TestExplodingEnemy.h"

#include "Sandbox/combat/effects/subsystems/ExplosionSubsystem.h"
#include "Sandbox/utilities/macros/null_checks.hpp"

ATestExplodingEnemy::ATestExplodingEnemy() {
    PrimaryActorTick.bCanEverTick = false;
}

bool ATestExplodingEnemy::attack_actor(AActor* target) {
    constexpr auto logger{NestedLogger<"attack_actor">()};

    RETURN_VALUE_IF_NULLPTR(target, false);
    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), false);

    // Check distance to target
    auto const distance_to_target{FVector::Dist(GetActorLocation(), target->GetActorLocation())};
    if (distance_to_target > combat_profile.melee_range) {
        logger.log_verbose(TEXT("Target out of explosion range: %.2f > %.2f"),
                           distance_to_target,
                           combat_profile.melee_range);
        return false;
    }

    // Trigger explosion at own location
    INIT_PTR_OR_RETURN_VALUE(
        explosion_subsystem, world->GetSubsystem<UExplosionSubsystem>(), false);
    auto const actor_location{GetActorLocation()};
    explosion_subsystem->spawn_explosion(actor_location, FRotator::ZeroRotator, explosion_config);

    logger.log_verbose(TEXT("TestExplodingEnemy exploded at location: %s targeting %s"),
                       *actor_location.ToString(),
                       *target->GetName());

    // Destroy self
    Destroy();

    return true;
}
