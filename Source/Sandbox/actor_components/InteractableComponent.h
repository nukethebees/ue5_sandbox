#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/interfaces/Interactable.h"

#include "InteractableComponent.generated.h"

UENUM()
enum class EInteractReady : uint8 { none_ready, some_ready, all_ready };

UENUM()
enum class EInteractTriggered : uint8 { none_triggered, some_triggered, all_triggered };

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
    EInteractReady can_interact(AActor* const interactor) const;
    UFUNCTION(BlueprintCallable)
    EInteractTriggered try_interact(AActor* const interactor);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    TArray<TScriptInterface<IInteractable>> linked_interactables;
};
