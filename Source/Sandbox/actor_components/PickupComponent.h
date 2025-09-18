#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Sandbox/interfaces/PickupOwner.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"

#include "PickupComponent.generated.h"

namespace ml {
inline static constexpr wchar_t UPickupComponentLogTag[]{TEXT("UPickupComponent")};
}

UCLASS()
class SANDBOX_API UPickupComponent
    : public UActorComponent
    , public ml::LogMsgMixin<ml::UPickupComponentLogTag> {
    GENERATED_BODY()
  public:
    UPickupComponent();
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type const EndPlayReason) override;

    virtual void execute_pickup_effect(AActor* target_actor) {};
  private:
    UFUNCTION()
    void on_overlap(UPrimitiveComponent* OverlappedComponent,
                    AActor* OtherActor,
                    UPrimitiveComponent* OtherComp,
                    int32 OtherBodyIndex,
                    bool bFromSweep,
                    FHitResult const& SweepResult);

    TWeakObjectPtr<UPrimitiveComponent> collision_component{};
    bool is_on_cooldown{false};
    FTimerHandle cooldown_timer_handle{};

    void reset_cooldown();
};
