#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnhancedInputComponent.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "Sandbox/constants/collision_channels.h"

#include "InteractorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractorComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"InteractorComponent"> {
    GENERATED_BODY()
  public:
    UInteractorComponent();
  public:
    void try_interact(FVector sweep_origin, FVector sweep_direction);
    void try_interact();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float interaction_cooldown{0.1f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float interaction_range{200.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float forward_offset{00.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float height_offset{00.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float capsule_radius{5.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float capsule_half_height{80.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    TEnumAsByte<ECollisionChannel> collision_channel{ml::collision::interaction};
  private:
    bool cooling_down{false};
    FTimerHandle cooldown_handle;
};
