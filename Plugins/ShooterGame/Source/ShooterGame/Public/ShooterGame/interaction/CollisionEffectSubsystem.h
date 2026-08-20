#pragma once

#include "ShooterGame/interaction/CollisionEffectSubsystemCore.h"
#include "ShooterGame/interaction/CollisionEffectSubsystemMixins.hpp"
#include "ShooterGame/interaction/CollisionPayloads.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <tuple>
#include <utility>

#include "CollisionEffectSubsystem.generated.h"

UCLASS()
class SHOOTERGAME_API UCollisionEffectSubsystem
    : public UWorldSubsystem {
    GENERATED_BODY()
  public:
    template <typename Payload>
    void add_payload(AActor& actor, Payload&& payload) {
        core_.add_payload(actor, std::forward<Payload>(payload), this);
    }

    UFUNCTION()
    void handle_collision_event(UPrimitiveComponent* overlapped_component,
                                AActor* other_actor,
                                UPrimitiveComponent* OtherComp,
                                int32 other_body_index,
                                bool from_sweep,
                                FHitResult const& sweep_result) {
        core_.handle_collision_event_(overlapped_component,
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
