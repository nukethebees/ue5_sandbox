#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/interfaces/Interactable.h"

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
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    UFUNCTION()
    void interact(AActor* const interactor) override;
  private:
    void stop_rotation();

    UPROPERTY(EditAnywhere, Category = "Rotation")
    FRotator rotation_rate{0.0f, 90.0f, 0.0f};

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float rotation_duration_seconds{2.0f};

    FTimerHandle rotation_timer_handle;
};
