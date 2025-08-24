#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/interfaces/Interactable.h"

#include "InteractableComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractableComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UInteractableComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    UFUNCTION(BlueprintCallable)
    void interact(AActor* const interactor);
    UFUNCTION(BlueprintCallable)
    bool can_interact(AActor* const interactor);
    UFUNCTION(BlueprintCallable)
    bool try_interact(AActor* const interactor);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    TArray<TScriptInterface<IInteractable>> linked_interactables;
};
