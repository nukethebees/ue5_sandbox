#pragma once

#include "Sandbox/interaction/CollisionEffectSubsystemCore.h"
#include "Sandbox/interaction/CollisionEffectSubsystemMixins.hpp"
#include "Sandbox/interaction/CollisionPayloads.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <tuple>

#include "CollisionEffectSubsystem.generated.h"

UCLASS()
class SANDBOX_API UCollisionEffectSubsystem
    : public UWorldSubsystem
    , public UCollisionEffectSubsystemMixins {
    GENERATED_BODY()
    friend class UCollisionEffectSubsystemMixins;
  public:
    UFUNCTION()
    void handle_collision_event(UPrimitiveComponent* overlapped_component,
                                AActor* other_actor,
                                UPrimitiveComponent* OtherComp,
                                int32 other_body_index,
                                bool from_sweep,
                                FHitResult const& sweep_result) {
        return handle_collision_event_(overlapped_component,
                                       other_actor,
                                       OtherComp,
                                       other_body_index,
                                       from_sweep,
                                       sweep_result);
    }
  private:
    UCollisionEffectSubsystemCore<FSpeedBoostPayload,
                                  FJumpIncreasePayload,
                                  FCoinPayload,
                                  FLandMinePayload>
        core_{};
};
