#include "Sandbox/players/npcs/TestExplodingEnemy.h"

#include "Engine/World.h"

#include "Sandbox/combat/explosion/ExplosionSubsystem.h"
#include "Sandbox/health/HealthComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ATestExplodingEnemy::ATestExplodingEnemy() {
    PrimaryActorTick.bCanEverTick = false;
}

bool ATestExplodingEnemy::attack_actor(AActor& target) {
    constexpr auto logger{NestedLogger<"attack_actor">()};

    check(health);

    // Check distance to target
    auto const distance_to_target{FVector::Dist(GetActorLocation(), target.GetActorLocation())};
    if (distance_to_target > combat_profile.melee_range) {
        logger.log_verbose(TEXT("Target out of explosion range: %.2f > %.2f"),
                           distance_to_target,
                           combat_profile.melee_range);
        return false;
    }

    this->health->kill();

    return true;
}
void ATestExplodingEnemy::handle_death() {
    Super::handle_death();

    TRY_INIT_PTR(world, GetWorld());
    explode(*world);
}
void ATestExplodingEnemy::explode(UWorld& world) {
    constexpr auto logger{NestedLogger<"explode">()};

    TRY_INIT_PTR(explosion_subsystem, world.GetSubsystem<UExplosionSubsystem>());
    auto const actor_location{GetActorLocation()};
    explosion_subsystem->spawn_explosion(actor_location, FRotator::ZeroRotator, explosion_config);
}
