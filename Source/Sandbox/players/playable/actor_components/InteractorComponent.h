#pragma once

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "EnhancedInputComponent.h"

#include "InteractorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractorComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"InteractorComponent", LogSandboxCharacter> {
    GENERATED_BODY()
  public:
    UInteractorComponent();
  public:
    void try_interact(FVector sweep_origin, FVector sweep_end);
    void try_interact(FVector sweep_origin, FRotator sweep_direction);
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
    TEnumAsByte<ECollisionChannel> collision_channel{ml::collision::interaction};
  private:
    bool cooling_down{false};
    FTimerHandle cooldown_handle;
};
