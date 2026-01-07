#include "Sandbox/combat/projectiles/actors/MassBulletSubsystemData.h"

AMassBulletSubsystemData::AMassBulletSubsystemData() {
    PrimaryActorTick.bCanEverTick = false;
}

void AMassBulletSubsystemData::add_bullet_type(UBulletDataAsset& bt) {
    for (auto const& bullet_type : bullet_types) {
        if (bullet_type == &bt) {
            return;
        }
    }

    bullet_types.Add(&bt);
}
