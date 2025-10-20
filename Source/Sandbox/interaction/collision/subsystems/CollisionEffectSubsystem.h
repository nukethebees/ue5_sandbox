#pragma once

#include <tuple>

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Sandbox/interaction/collision/data/CollisionPayloads.h"
#include "Sandbox/interaction/collision/mixins/CollisionEffectSubsystemMixins.hpp"
#include "Sandbox/interaction/collision/subsystems/CollisionEffectSubsystemCore.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

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
