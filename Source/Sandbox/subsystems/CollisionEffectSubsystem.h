#pragma once

#include <tuple>

#include "CoreMinimal.h"
#include "Sandbox/subsystems/CollisionEffectSubsystemCore.h"
#include "Sandbox/data/collision/CollisionPayloads.h"
#include "Sandbox/mixins/CollisionEffectSubsystemMixins.hpp"
#include "Subsystems/WorldSubsystem.h"

#include "CollisionEffectSubsystem.generated.h"

UCLASS()
class SANDBOX_API UCollisionEffectSubsystem
    : public UWorldSubsystem
    , public UCollisionEffectSubsystemMixins {
    GENERATED_BODY()
    friend class UCollisionEffectSubsystemMixins;
  public:
    UFUNCTION()
    void handle_collision_event(UPrimitiveComponent* OverlappedComponent,
                                AActor* OtherActor,
                                UPrimitiveComponent* OtherComp,
                                int32 OtherBodyIndex,
                                bool bFromSweep,
                                FHitResult const& SweepResult) {
        return handle_collision_event_(
            OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
    }
  private:
    UCollisionEffectSubsystemCore<FSpeedBoostPayload,
                                  FJumpIncreasePayload,
                                  FCoinPayload,
                                  FLandMinePayload>
        core_{};
};
