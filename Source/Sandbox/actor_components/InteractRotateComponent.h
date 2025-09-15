#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/interfaces/Activatable.h"
#include "Sandbox/interfaces/Interactable.h"

#include "InteractRotateComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractRotateComponent
    : public UActorComponent
    , public IInteractable
    , public IActivatable {
    GENERATED_BODY()
  public:
    UInteractRotateComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    UFUNCTION()
    void interact(AActor* interactor) override;
    UFUNCTION()
    void trigger_activation(AActor* instigator) override;
  private:
    void start_rotation(AActor* instigator);
    void stop_rotation();

    UPROPERTY(EditAnywhere, Category = "Rotation")
    FRotator rotation_rate{0.0f, 90.0f, 0.0f};

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float rotation_duration_seconds{2.0f};

    FTimerHandle rotation_timer_handle;
};
