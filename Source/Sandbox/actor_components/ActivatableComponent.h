#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/interfaces/Activatable.h"

#include "ActivatableComponent.generated.h"

UENUM()
enum class EActivateReady : uint8 { none_ready, some_ready, all_ready };

UENUM()
enum class EActivateTriggered : uint8 { none_triggered, some_triggered, all_triggered };

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UActivatableComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UActivatableComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    UFUNCTION(BlueprintCallable)
    void trigger_activation(AActor* instigator);
    UFUNCTION(BlueprintCallable)
    EActivateReady can_activate(AActor const* instigator) const;
    UFUNCTION(BlueprintCallable)
    EActivateTriggered try_activate(AActor* instigator);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activation")
    TArray<TScriptInterface<IActivatable>> linked_activatables;
};
