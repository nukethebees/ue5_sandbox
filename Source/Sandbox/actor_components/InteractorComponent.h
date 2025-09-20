#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "InteractorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractorComponent
    : public UActorComponent
    , public ml::LogMsgMixin<TEXT("InteractorComponent")> {
    GENERATED_BODY()
  public:
    UInteractorComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    UFUNCTION(BlueprintCallable)
    void try_interact();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> interact_action;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float interaction_cooldown{0.1f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float interaction_range{200.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float forward_offset{50.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float height_offset{50.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float capsule_radius{40.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float capsule_half_height{80.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    TEnumAsByte<ECollisionChannel> collision_channel{ECC_GameTraceChannel1};
  private:
    bool cooling_down{false};
    FTimerHandle cooldown_handle;
};
