#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/interfaces/Interactable.h"
#include "Sandbox/actor_components/ActivatorComponent.h"

#include "InteractToActivateComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractToActivateComponent
    : public UActorComponent
    , public IInteractable {
    GENERATED_BODY()
  public:
    UInteractToActivateComponent();
  protected:
    virtual void BeginPlay() override;
    UFUNCTION()
    void interact(AActor* interactor) override;
  private:
    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Interaction",
              meta = (AllowPrivateAccess = "true"))
    UActivatorComponent* activator{nullptr};
};
