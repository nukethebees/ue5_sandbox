#include "Sandbox/combat/weapons/actors/MassBulletWeapon.h"

#include "Sandbox/combat/projectiles/actors/MassBulletSubsystemData.h"
#include "Sandbox/combat/projectiles/actors/MassBulletVisualizationActor.h"
#include "Sandbox/environment/utilities/world.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void AMassBulletWeapon::BeginPlay() {
    Super::BeginPlay();
}

#if WITH_EDITOR
void AMassBulletWeapon::OnConstruction(FTransform const& Transform) {
    TRY_INIT_PTR(world, GetWorld());

    ml::get_or_create_actor_singleton<AMassBulletVisualizationActor>(*world);

    if (auto* actor{ml::get_or_create_actor_singleton<AMassBulletSubsystemData>(*world)}) {
        RETURN_IF_NULLPTR(actor);
        RETURN_IF_NULLPTR(bullet_data);
        actor->add_bullet_type(*bullet_data);
    }
}
#endif
