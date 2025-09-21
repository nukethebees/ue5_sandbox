#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/interfaces/Interactable.h"
#include "Sandbox/data/trigger/FRotatePayload.h"
#include "Sandbox/data/trigger/TriggerableId.h"

#include "InteractRotateComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractRotateComponent
    : public UActorComponent
    , public IInteractable {
    GENERATED_BODY()
  public:
    UInteractRotateComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    UFUNCTION()
    void interact(AActor* interactor) override;

    // Trigger this component's rotation behavior
    void trigger_rotation(AActor* instigator = nullptr);
  private:
    UPROPERTY(EditAnywhere, Category = "Rotation")
    FRotatePayload rotation_payload{};

    TriggerableId triggerable_id{TriggerableId::invalid()};
};
