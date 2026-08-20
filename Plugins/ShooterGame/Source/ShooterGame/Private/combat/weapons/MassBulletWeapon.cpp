#include "ShooterGame/combat/weapons/MassBulletWeapon.h"

#include <SandboxCoreEngine/actor_utils.h>
#include "ShooterGame/combat/bullets/MassBulletSubsystemData.h"
#include "ShooterGame/combat/bullets/MassBulletVisualizationActor.h"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

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
