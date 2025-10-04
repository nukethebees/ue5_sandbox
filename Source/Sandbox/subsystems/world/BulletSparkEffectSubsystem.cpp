#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Sandbox/actors/BulletSparkEffectManagerActor.h"

void UBulletSparkEffectSubsystem::add_impact(FVector const& location, FRotator const& rotation) {}

void UBulletSparkEffectSubsystem::register_actor(ABulletSparkEffectManagerActor* actor) {
    manager_actor = actor;
}

void UBulletSparkEffectSubsystem::Tick(float DeltaTime) {}

TStatId UBulletSparkEffectSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UBulletSparkEffectSubsystem, STATGROUP_Tickables);
}
